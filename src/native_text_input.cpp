#include "native_text_input.hpp"

#include "runtime_log.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace erui::native {
namespace {

constexpr std::size_t kNativeFunctionStorageSize = 0x40;
constexpr std::size_t kTextReferencesStorageSize = 0x240;
constexpr std::size_t kPropertyCollectionOffset = 0x1268;
constexpr std::size_t kPropertyRowStride = 0x88;
constexpr std::size_t kPropertyCountOffset = 0x1AF0;
constexpr std::size_t kPropertyControllerOffset = 0x78;
constexpr std::size_t kControllerBoundValueOffset = 0x1D0;
constexpr std::size_t kMaximumPropertyRows = 16;
constexpr std::uintptr_t kOptionSettingDialogVtableRva = 0x2B14888;
constexpr std::uintptr_t kPadSettingDialogVtableRva = 0x2B16DD8;
constexpr std::uintptr_t kEditPropertyVtableRva = 0x2AD73D0;
constexpr std::uintptr_t kTextInputControllerVtableRva = 0x2B1B128;
constexpr std::uintptr_t kSoftwareKeyboardJobVtableRva = 0x2AC5AD0;
constexpr std::size_t kSoftwareKeyboardJobConfigOffset = 0x60;
constexpr std::size_t kConfigPrimaryLimitOffset = 0x60;
constexpr std::size_t kConfigSecondaryLimitOffset = 0x6C;
constexpr std::uint32_t kCharacterEditorNativeLimit = 16;
constexpr std::int32_t kCharacterEditorTextOriginCorrectionX = 8;
constexpr std::uint32_t kDiagnosticLogLimit = 16;

static_assert(sizeof(std::function<void()>) == kNativeFunctionStorageSize);
static_assert(sizeof(std::function<bool()>) == kNativeFunctionStorageSize);
static_assert(sizeof(wchar_t) == sizeof(std::uint16_t));

struct NativeBinding {
    erui::detail::TextInputState* state{};
    GameAddresses addresses{};
    std::array<wchar_t,
        erui::detail::text_input_maximum_length + 1> shadow{};
    std::uint64_t applied_revision{};
    bool native_value_constructed{};
    std::atomic_uint32_t activations{};
    std::atomic_uint32_t completions{};
    std::atomic_uint32_t patch_failures{};
    std::atomic_bool force_synchronize{};
};

std::vector<std::unique_ptr<NativeBinding>> g_bindings{};
std::vector<std::pair<const void*, NativeBinding*>> g_bound_lookup{};
std::vector<std::pair<const erui::detail::TextInputState*, NativeBinding*>>
    g_state_lookup{};
std::atomic_bool g_disabled{};

template <typename Key>
NativeBinding* lookup_binding(
    const std::vector<std::pair<const Key*, NativeBinding*>>& lookup,
    const Key* key) noexcept {
    const auto found = std::lower_bound(
        lookup.begin(), lookup.end(), key,
        [](const auto& entry, const Key* candidate) {
            return std::less<const Key*>{}(entry.first, candidate);
        });
    return found != lookup.end() && found->first == key
        ? found->second
        : nullptr;
}

NativeBinding* binding_from_native_value(const void* value) noexcept {
    return lookup_binding(g_bound_lookup, value);
}

NativeBinding* binding_from_state(
    const erui::detail::TextInputState* state) noexcept {
    return lookup_binding(g_state_lookup, state);
}

bool valid_utf16(std::wstring_view value) noexcept {
    for (std::size_t index = 0; index < value.size(); ++index) {
        const std::uint16_t unit = static_cast<std::uint16_t>(value[index]);
        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            if (++index >= value.size()) return false;
            const std::uint16_t low =
                static_cast<std::uint16_t>(value[index]);
            if (low < 0xDC00u || low > 0xDFFFu) return false;
        } else if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            return false;
        }
    }
    return true;
}

struct DecodedNativeText {
    std::array<wchar_t,
        erui::detail::text_input_maximum_length + 1> units{};
    std::size_t length{};
};

bool copy_native_menu_string(
    const NativeBinding& binding,
    DecodedNativeText& output) noexcept {
#if defined(_MSC_VER)
    __try {
#endif
        const auto* value = binding.state->native_value.data();
        const wchar_t* text{};
        std::memcpy(&text, value, sizeof(text));
        std::size_t length{};
        if (text) {
            const std::size_t bound =
                static_cast<std::size_t>(binding.state->maximum_length()) + 1;
            while (length < bound && text[length] != L'\0') ++length;
            if (length == bound) return false;
        } else {
            std::uint64_t native_length{};
            std::uint64_t native_capacity{};
            std::memcpy(&native_length, value + 0x20, sizeof(native_length));
            std::memcpy(&native_capacity, value + 0x28, sizeof(native_capacity));
            if (native_length > binding.state->maximum_length() ||
                native_capacity < native_length) {
                return false;
            }
            length = static_cast<std::size_t>(native_length);
            text = reinterpret_cast<const wchar_t*>(value + 0x10);
            if (native_capacity > 7) {
                std::memcpy(&text, value + 0x10, sizeof(text));
            }
            if (!text || text[length] != L'\0') return false;
        }
        if (length >= output.units.size()) return false;
        std::memcpy(output.units.data(), text, length * sizeof(wchar_t));
        output.units[length] = L'\0';
        output.length = length;
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool decode_native_menu_string(
    const NativeBinding& binding,
    std::wstring& output) noexcept {
    DecodedNativeText copied{};
    if (!copy_native_menu_string(binding, copied) ||
        !valid_utf16({copied.units.data(), copied.length})) {
        return false;
    }
    try {
        output.assign(copied.units.data(), copied.length);
        return true;
    } catch (...) {
        return false;
    }
}

bool reconstruct_native_value(NativeBinding& binding) noexcept {
#if defined(_MSC_VER)
    __try {
#endif
        if (binding.native_value_constructed) {
            binding.native_value_constructed = false;
            binding.addresses.native_menu_string_destructor(
                binding.state->native_value.data());
        }
        binding.addresses.native_menu_string_borrowed_constructor(
            binding.state->native_value.data(), binding.shadow.data());
        binding.native_value_constructed = true;
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool destroy_native_value(NativeBinding* binding) noexcept {
    if (!binding || !binding->native_value_constructed ||
        !binding->addresses.native_menu_string_destructor) {
        return true;
    }
#if defined(_MSC_VER)
    __try {
#endif
        binding->native_value_constructed = false;
        binding->addresses.native_menu_string_destructor(
            binding->state->native_value.data());
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool synchronize_binding(NativeBinding& binding, bool force) noexcept {
    try {
        const erui::detail::TextInputState::Snapshot snapshot =
            binding.state->snapshot();
        if (!force && binding.native_value_constructed &&
            binding.applied_revision == snapshot.revision) {
            return true;
        }
        if (snapshot.value.size() > binding.state->maximum_length() ||
            snapshot.value.size() >= binding.shadow.size()) {
            return false;
        }

        std::fill(binding.shadow.begin(), binding.shadow.end(), L'\0');
        std::copy(snapshot.value.begin(), snapshot.value.end(),
            binding.shadow.begin());
        if (!reconstruct_native_value(binding)) return false;
        binding.applied_revision = snapshot.revision;
        return true;
    } catch (...) {
        return false;
    }
}

void completion_callback(NativeBinding& binding) noexcept {
    try {
        std::wstring value{};
        if (!decode_native_menu_string(binding, value)) {
            erui::detail::logf(
                erui::LogLevel::error,
                "TextInput completion produced an invalid native value; keeping the previous host value");
            binding.force_synchronize.store(true, std::memory_order_release);
            return;
        }

        const erui::detail::TextInputState::Snapshot before =
            binding.state->snapshot();
        if (!binding.state->commit_native(value)) {
            erui::detail::logf(
                erui::LogLevel::error,
                "TextInput completion exceeded its declared maximum; keeping the previous host value");
            binding.force_synchronize.store(true, std::memory_order_release);
            return;
        }
        // The game has just written the confirmed value into the bound native
        // string and invokes us from inside that write-back callback. Normalize
        // the borrowed shadow on the next page frame, outside this native
        // callback stack. Force the pass even when the value was unchanged and
        // the canonical revision therefore did not advance.
        binding.force_synchronize.store(true, std::memory_order_release);

        const std::uint32_t completion =
            binding.completions.fetch_add(1, std::memory_order_relaxed) + 1;
        if (completion <= kDiagnosticLogLimit) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "TextInput completion: state=%p changed=%d length=%zu maximum=%u",
                static_cast<void*>(binding.state),
                before.value == value ? 0 : 1,
                value.size(),
                static_cast<unsigned>(binding.state->maximum_length()));
        }
    } catch (...) {
        binding.force_synchronize.store(true, std::memory_order_release);
        erui::detail::logf(
            erui::LogLevel::error,
            "TextInput completion could not allocate host-owned state; restoring the previous value on the next page frame");
    }
}

struct LimitPatchObservation {
    void* job{};
    std::uint32_t before_primary{};
    std::uint32_t before_secondary{};
};

bool apply_maximum_length(
    void* destination,
    void* factory_result,
    NativeBinding& binding,
    LimitPatchObservation& observation) noexcept {
#if defined(_MSC_VER)
    __try {
#endif
        if (!destination || factory_result != destination ||
            !binding.addresses.game_image_base) {
            return false;
        }
        std::memcpy(&observation.job, destination, sizeof(observation.job));
        if (!observation.job) return false;

        void* vtable{};
        std::memcpy(&vtable, observation.job, sizeof(vtable));
        if (vtable != binding.addresses.game_image_base +
                kSoftwareKeyboardJobVtableRva) {
            return false;
        }

        auto* config = static_cast<std::byte*>(observation.job) +
            kSoftwareKeyboardJobConfigOffset;
        std::memcpy(&observation.before_primary,
            config + kConfigPrimaryLimitOffset,
            sizeof(observation.before_primary));
        std::memcpy(&observation.before_secondary,
            config + kConfigSecondaryLimitOffset,
            sizeof(observation.before_secondary));
        if (observation.before_primary != kCharacterEditorNativeLimit ||
            observation.before_secondary != kCharacterEditorNativeLimit) {
            return false;
        }

        const std::uint32_t maximum = binding.state->maximum_length();
        std::memcpy(config + kConfigPrimaryLimitOffset,
            &maximum, sizeof(maximum));
        std::memcpy(config + kConfigSecondaryLimitOffset,
            &maximum, sizeof(maximum));

        std::uint32_t primary{};
        std::uint32_t secondary{};
        std::memcpy(&primary, config + kConfigPrimaryLimitOffset,
            sizeof(primary));
        std::memcpy(&secondary, config + kConfigSecondaryLimitOffset,
            sizeof(secondary));
        return primary == maximum && secondary == maximum;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

void* __fastcall production_editor_factory(
    void* destination,
    void* parent,
    void* bound_text,
    void* native_completion_action,
    const std::int32_t* screen_position) {
    NativeBinding* const binding = binding_from_native_value(bound_text);
    if (!binding || !binding->addresses.text_input_editor_factory) {
        return destination;
    }

    std::int32_t corrected_position[2]{};
    const std::int32_t* effective_position = screen_position;
    if (screen_position) {
        corrected_position[0] = screen_position[0] +
            kCharacterEditorTextOriginCorrectionX;
        corrected_position[1] = screen_position[1];
        effective_position = corrected_position;
    }

    void* result = binding->addresses.text_input_editor_factory(
        destination, parent, bound_text, native_completion_action,
        effective_position);
    LimitPatchObservation observation{};
    if (!apply_maximum_length(
            destination, result, *binding, observation)) {
        const std::uint32_t failures =
            binding->patch_failures.fetch_add(1, std::memory_order_relaxed) + 1;
        if (failures == 1) {
            erui::detail::logf(
                erui::LogLevel::error,
                "TextInput maximum patch rejected: state=%p job=%p baseline=(%u,%u) requested=%u",
                static_cast<void*>(binding->state),
                observation.job,
                static_cast<unsigned>(observation.before_primary),
                static_cast<unsigned>(observation.before_secondary),
                static_cast<unsigned>(binding->state->maximum_length()));
        }
    } else {
        const std::uint32_t activation =
            binding->activations.fetch_add(1, std::memory_order_relaxed) + 1;
        if (activation <= kDiagnosticLogLimit) {
            erui::detail::logf(
                erui::LogLevel::trace,
                "TextInput activation: state=%p job=%p maximum=%u",
                static_cast<void*>(binding->state),
                observation.job,
                static_cast<unsigned>(binding->state->maximum_length()));
        }
    }
    return result;
}

bool page_preconditions(
    void* page,
    const GameAddresses& addresses,
    std::size_t& row_count) noexcept {
#if defined(_MSC_VER)
    __try {
#endif
        if (!page || !addresses.game_image_base ||
            !addresses.text_input_complete()) {
            return false;
        }
        void* page_vtable{};
        std::memcpy(&page_vtable, page, sizeof(page_vtable));
        if (page_vtable != addresses.game_image_base +
                kOptionSettingDialogVtableRva &&
            page_vtable != addresses.game_image_base +
                kPadSettingDialogVtableRva) {
            return false;
        }
        std::memcpy(&row_count,
            static_cast<const std::byte*>(page) + kPropertyCountOffset,
            sizeof(row_count));
        return row_count < kMaximumPropertyRows;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool postconditions(
    void* page,
    const GameAddresses& addresses,
    const NativeBinding& binding,
    std::size_t previous_count) noexcept {
#if defined(_MSC_VER)
    __try {
#endif
        std::size_t current_count{};
        std::memcpy(&current_count,
            static_cast<const std::byte*>(page) + kPropertyCountOffset,
            sizeof(current_count));
        if (current_count != previous_count + 1) return false;

        auto* native_row = static_cast<std::byte*>(page) +
            kPropertyCollectionOffset +
            previous_count * kPropertyRowStride;
        void* row_vtable{};
        void* controller{};
        std::memcpy(&row_vtable, native_row, sizeof(row_vtable));
        std::memcpy(&controller,
            native_row + kPropertyControllerOffset,
            sizeof(controller));
        if (row_vtable != addresses.game_image_base +
                kEditPropertyVtableRva || !controller) {
            return false;
        }

        void* controller_vtable{};
        void* bound_value{};
        std::memcpy(&controller_vtable, controller,
            sizeof(controller_vtable));
        std::memcpy(&bound_value,
            static_cast<const std::byte*>(controller) +
                kControllerBoundValueOffset,
            sizeof(bound_value));
        return controller_vtable == addresses.game_image_base +
                kTextInputControllerVtableRva &&
            bound_value == binding.state->native_value.data();
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool construct_native_text_input_row(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    NativeBinding& binding,
    std::size_t previous_count,
    std::function<void()>* completion_action,
    std::function<bool()>* disabled_predicate,
    unsigned long& out_seh_code) noexcept {
    alignas(16) std::array<std::byte, kTextReferencesStorageSize>
        text_references{};
    alignas(16) std::array<std::byte,
        erui::detail::native_menu_string_storage_size> initial_text{};
    alignas(16) std::array<std::byte,
        erui::detail::native_menu_string_storage_size> placeholder_text{};
    alignas(16) std::array<std::byte, kNativeFunctionStorageSize>
        editor_factory{};

    out_seh_code = 0;
#if defined(_MSC_VER)
    __try {
#endif
        addresses.native_menu_string_constructor(initial_text.data());
        addresses.native_menu_string_borrowed_constructor(
            placeholder_text.data(), binding.state->placeholder().c_str());
        addresses.text_input_editor_factory_builder(
            editor_factory.data(), &production_editor_factory);
        addresses.text_ref_label(text_references.data(), row.label_id);
        addresses.text_ref_help(text_references.data() + 0x38, row.help_id);

        addresses.text_input_row_producer(
            page,
            text_references.data(),
            binding.state->native_value.data(),
            editor_factory.data(),
            initial_text.data(),
            placeholder_text.data(),
            completion_action,
            disabled_predicate,
            1);

        addresses.destroy_text_references(text_references.data());
        addresses.native_menu_string_destructor(placeholder_text.data());
        addresses.native_menu_string_destructor(initial_text.data());
        return postconditions(page, addresses, binding, previous_count);
#if defined(_MSC_VER)
    } __except (
        out_seh_code = GetExceptionCode(),
        EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

} // namespace

bool prepare_native_text_inputs(
    const erui::detail::CompiledMenu& menu,
    const GameAddresses& addresses) noexcept {
    reset_native_text_inputs();
    try {
        g_bindings.reserve(menu.modeled_text_input_count);
        g_bound_lookup.reserve(menu.modeled_text_input_count);
        g_state_lookup.reserve(menu.modeled_text_input_count);

        for (const erui::detail::CompiledPage& page : menu.pages) {
            if (!menu.page_reachable(&page - menu.pages.data())) continue;
            for (const erui::detail::CompiledRow& row : page.rows) {
                if (row.kind != erui::RowKind::text_input) continue;
                if (!row.text_input_state || !addresses.text_input_complete()) {
                    return false;
                }
                const bool duplicate = std::any_of(
                    g_bindings.begin(), g_bindings.end(),
                    [&row](const auto& binding) {
                        return binding->state == row.text_input_state;
                    });
                if (duplicate) {
                    erui::detail::logf(
                        erui::LogLevel::error,
                        "TextInput state was assigned to more than one logical row");
                    return false;
                }
                auto binding = std::make_unique<NativeBinding>();
                binding->state = row.text_input_state;
                binding->addresses = addresses;
                NativeBinding* raw = binding.get();
                g_bindings.push_back(std::move(binding));
                g_bound_lookup.emplace_back(
                    raw->state->native_value.data(), raw);
                g_state_lookup.emplace_back(raw->state, raw);
            }
        }
        const auto by_pointer = [](const auto& left, const auto& right) {
            return std::less<const void*>{}(
                static_cast<const void*>(left.first),
                static_cast<const void*>(right.first));
        };
        std::sort(g_bound_lookup.begin(), g_bound_lookup.end(), by_pointer);
        std::sort(g_state_lookup.begin(), g_state_lookup.end(), by_pointer);
        return g_bindings.size() == menu.modeled_text_input_count;
    } catch (...) {
        reset_native_text_inputs();
        return false;
    }
}

void reset_native_text_inputs() noexcept {
    for (const auto& binding : g_bindings) {
        if (!destroy_native_value(binding.get())) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "TextInput native value cleanup raised a native exception");
        }
    }
    g_bound_lookup.clear();
    g_state_lookup.clear();
    g_bindings.clear();
    g_disabled.store(false, std::memory_order_release);
}

void synchronize_pending_text_inputs() noexcept {
    if (g_disabled.load(std::memory_order_acquire)) return;
    for (const auto& binding : g_bindings) {
        const bool force = binding->force_synchronize.exchange(
            false, std::memory_order_acq_rel);
        if (!synchronize_binding(*binding, force)) {
            g_disabled.store(true, std::memory_order_release);
            erui::detail::logf(
                erui::LogLevel::error,
                "TextInput native synchronization failed; disabled for this process");
            return;
        }
    }
}

bool add_text_input(
    void* page,
    const GameAddresses& addresses,
    const erui::detail::CompiledRow& row,
    bool& faulted) noexcept {
    faulted = false;
    if (g_disabled.load(std::memory_order_acquire) ||
        !row.text_input_state) {
        return false;
    }
    NativeBinding* const binding = binding_from_state(row.text_input_state);
    if (!binding) return false;

    std::size_t previous_count{};
    if (!page_preconditions(page, addresses, previous_count) ||
        !synchronize_binding(*binding, false)) {
        return false;
    }

    try {
        std::function<void()> completion_action{
            [binding]() noexcept { completion_callback(*binding); }};
        std::function<bool()> disabled_predicate{};
        unsigned long seh_exception_code{};
        const bool constructed = construct_native_text_input_row(
            page,
            addresses,
            row,
            *binding,
            previous_count,
            &completion_action,
            &disabled_predicate,
            seh_exception_code);
        if (!constructed) {
            g_disabled.store(true, std::memory_order_release);
            faulted = seh_exception_code != 0;
            erui::detail::logf(
                erui::LogLevel::error,
                "TextInput row construction failed%s; disabled for this process",
                faulted ? " with a native exception" : " post-validation");
            return false;
        }
        return true;
    } catch (...) {
        g_disabled.store(true, std::memory_order_release);
        erui::detail::logf(
            erui::LogLevel::error,
            "TextInput callback construction failed; disabled for this process");
        return false;
    }
}

} // namespace erui::native
