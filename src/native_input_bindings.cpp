#include "native_input_bindings.hpp"

#include "binding_runtime_state.hpp"
#include "game_build_profiles.hpp"
#include "input_binding_model.hpp"
#include "menu_compiler.hpp"
#include "module.hpp"
#include "native_dialog.hpp"
#include "runtime_log.hpp"
#include "runtime_state.hpp"

#include <Windows.h>
#include <safetyhook.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace erui::native {
namespace {

// Every object offset and vtable in this backend is exact-build evidence. A
// recognized executable identity and its matching profile are therefore
// mandatory even when all entry prologues still happen to match.
constexpr std::string_view kBuildKeySettingListPattern =
    "44 0F BE 42 08 45 85 C0 74 ?? 41 83 F8 01 75 ?? "
    "E9 ?? ?? ?? ?? E9 ?? ?? ?? ?? C3";
constexpr std::string_view kConstructSpacerPattern =
    "48 89 4C 24 08 53 48 83 EC 30 "
    "48 C7 44 24 28 FE FF FF FF";
constexpr std::string_view kAppendKeySettingPattern =
    "48 89 5C 24 08 57 48 83 EC 20 48 8B D9 48 8B FA 48 8B 49 10";
constexpr std::string_view kConstructKeySettingPattern =
    "48 89 4C 24 08 53 48 83 EC 30 "
    "48 C7 44 24 20 FE FF FF FF 48 8B D9 "
    "48 8D 05 ?? ?? ?? ?? 48 89 01 "
    "48 8D 05 ?? ?? ?? ?? 48 89 01 "
    "89 51 08 4C 89 41 10";
constexpr std::string_view kPollKeyCapturePattern =
    "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 "
    "57 48 83 EC 20 48 8B F9 49 8B F1 49 8B C9 "
    "49 8B E8 48 8B DA";
constexpr std::string_view kClearKeySettingPattern =
    "48 89 5C 24 08 57 48 83 EC 30 48 8B D9 48 8B FA "
    "48 8D 4C 24 20 E8 ?? ?? ?? ?? 44 8B 43 08";
constexpr std::string_view kRefreshKeyConflictPattern =
    "40 55 56 57 41 54 41 55 41 56 41 57 "
    "48 8D 6C 24 D9 48 81 EC C0 00 00 00 "
    "48 C7 45 1F FE FF FF FF";
constexpr std::string_view kWriteMenuConfigPattern =
    "45 8B D0 83 FA 35 77 ?? 48 63 C2 4D 8B C1 "
    "48 8D 14 80 48 8D 0C 91 41 8B D2 48 83 C1 30 "
    "E9 ?? ?? ?? ?? C3";
constexpr std::string_view kWriteBindingValuePattern =
    "48 89 5C 24 08 57 48 83 EC 20 49 8B D8 48 8B F9 "
    "85 D2 74 ?? 83 EA 01 74 ?? 83 FA 01";
constexpr std::string_view kInputTokenPhysicalIdPattern =
    "40 53 48 83 EC 20 8B 41 04 48 8B D9 83 C9 FF 85 C0 "
    "75 ?? 44 8B 0B EB ?? 44 8B C9";
constexpr std::string_view kInputTokenAnalogPattern =
    "83 79 04 00 75 ?? 8B 09 E9 ?? ?? ?? ?? 83 C9 FF E9 ?? ?? ?? ??";
constexpr std::string_view kGetPlayerInputPattern =
    "48 83 EC 28 44 8B CA 4C 8B C1 4C 3B 49 70 72 07 "
    "33 C0 48 83 C4 28 C3 48 8D 41 48 48 89 5C 24 20 "
    "48 F7 D8 4E 8D 1C CD 00 00 00 00 83 E0 07 "
    "48 8D 1D 62 0B B2 03";
constexpr std::string_view kQueryInputStatesPattern =
    "48 8B 49 10 E9 ?? ?? ?? ?? 90 ?? ?? ?? ?? ?? ?? "
    "40 53 48 81 EC 80 00 00 00 8B 84 24 B0 00 00 00 "
    "48 8B 59 38";
constexpr std::string_view kInputManagerUpdatePattern =
    "48 89 5C 24 20 55 56 57 41 56 41 57 48 83 EC 20 "
    "45 33 C9 4C 8B F2 4C 8B F9 41 8B E9 44 89 4C 24 50";
constexpr std::string_view kSoftwareKeyboardJobConstructPattern =
    "48 89 4C 24 08 53 55 56 57 41 56 48 83 EC 30 "
    "48 C7 44 24 20 FE FF FF FF 49 8B F1 49 8B F8 "
    "48 8B DA 4C 8B F1 E8 ?? ?? ?? ?? 90 "
    "48 8D 05 ?? ?? ?? ?? 49 89 06";
constexpr std::string_view kSoftwareKeyboardJobDestroyPattern =
    "48 89 4C 24 08 57 48 83 EC 30 "
    "48 C7 44 24 20 FE FF FF FF 48 89 5C 24 50 "
    "48 89 74 24 58 48 8B D9 48 8D B9 68 01 00 00 "
    "48 89 7C 24 48";
constexpr std::string_view kTextInputDialogConstructPattern =
    "48 8B C4 48 89 48 08 55 56 57 41 56 41 57 "
    "48 81 EC 90 00 00 00 48 C7 44 24 20 FE FF FF FF "
    "48 89 58 10 4D 8B F9 49 8B F8 48 8B DA 48 8B F1 "
    "BA 44 00 00 00 48 8D 48 20";
constexpr std::string_view kTextInputDialogDestroyPattern =
    "48 89 4C 24 08 57 48 83 EC 30 "
    "48 C7 44 24 20 FE FF FF FF 48 89 5C 24 50 "
    "48 8B D9 48 8D B9 78 0B 00 00 48 89 7C 24 48 "
    "48 8B 4F 38";

constexpr std::size_t kKeySettingSize = 0x50;
constexpr std::size_t kNativeDefinitionSize = 0x18;
constexpr std::size_t kDialogToListOffset = 0x1268;
constexpr std::size_t kDialogModeOffset = 0x1260;
constexpr std::size_t kListToConfigOffset = 0x28;
constexpr std::size_t kListVectorOffset = 0x08;
constexpr std::size_t kListBeginOffset = 0x10;
constexpr std::size_t kListEndOffset = 0x18;
constexpr std::size_t kListCapacityOffset = 0x20;
constexpr std::size_t kConfigModeOffset = 0x08;
constexpr std::size_t kItemKindOffset = 0x08;
constexpr std::size_t kItemDefinitionOffset = 0x10;
constexpr std::size_t kItemBindingOffset = 0x18;
constexpr std::size_t kItemLatestTokenOffset = 0x24;
constexpr std::size_t kItemConflictOffset = 0x30;
constexpr std::size_t kItemKeyboardTokenOffset = 0x34;
constexpr std::int32_t kUnownedNativeActionId = -1;
constexpr std::uint8_t kControllerConfigMode = 0;
constexpr std::uint8_t kKeyboardMouseConfigMode = 1;
constexpr std::size_t kConfigModeCount = 2;
constexpr std::uint32_t kControllerKind = 0;
constexpr std::uint32_t kKeyboardKind = 1;
constexpr std::uint32_t kMouseKind = 2;

static_assert(
    ERUI_INPUT_DEVICE_CONTROLLER == runtime_binding_device_bit(0));
static_assert(ERUI_INPUT_DEVICE_KEYBOARD == runtime_binding_device_bit(1));
static_assert(ERUI_INPUT_DEVICE_MOUSE == runtime_binding_device_bit(2));

constexpr bool is_category_kind_pair(
    bool controller_mode,
    std::uint32_t primary,
    std::uint32_t secondary) noexcept {
    return primary ==
            (controller_mode ? kControllerKind : kKeyboardKind) &&
        secondary == kKeyboardKind;
}

constexpr bool is_action_kind_pair(
    bool controller_mode,
    std::uint32_t primary,
    std::uint32_t secondary) noexcept {
    return primary ==
            (controller_mode ? kControllerKind : kKeyboardKind) &&
        secondary ==
            (controller_mode ? kKeyboardKind : kMouseKind);
}

static_assert(is_category_kind_pair(true, 0, 1));
static_assert(is_action_kind_pair(true, 0, 1));
static_assert(is_category_kind_pair(false, 1, 1));
static_assert(!is_action_kind_pair(false, 1, 1));
static_assert(!is_category_kind_pair(false, 1, 2));
static_assert(is_action_kind_pair(false, 1, 2));
constexpr std::size_t kMaximumBindingSections = 4096;
constexpr std::size_t kMaximumBindingActions = 4096;
constexpr std::size_t kMaximumTrackedTextEditors = 16;
constexpr std::size_t kMaximumIdentifierBytes = 255;
constexpr float kAnalogActivationEpsilon = 0.0001f;
constexpr std::array<std::uint32_t, 5> kEmptyBindingValue = {
    0xFFFFFFFFu, 0xFFFFFFFFu, 0u, 0u, 0u};

enum class BindingBuilderStage : std::uint32_t {
    live_list = 1,
    definitions = 2,
    scratch = 3,
    append = 4,
    tail_validation = 5,
};

struct DefinitionScanTelemetry {
    std::uint32_t pairs_total{};
    std::uint32_t kind_pairs{};
    std::uint32_t category_sentinels{};
    std::uint32_t category_primary_null{};
    std::uint32_t category_spacers{};
    std::uint32_t action_ids_in_range{};
    std::uint32_t action_primary_nonnull{};
    std::uint32_t action_secondary_shape{};
    std::uint32_t action_flags{};
    std::uint32_t first_pair_kinds{0xFFFFFFFFu};
    std::uint32_t first_action_id{0xFFFFFFFFu};
    std::uint32_t first_category_shape{};
    std::uint32_t first_action_shape{};
    std::uint64_t first_category_flags_low{};
    std::uint64_t first_category_flags_high{};
    std::uint64_t first_action_flags_low{};
    std::uint64_t first_action_flags_high{};
};

struct KeyBindingDefinition {
    std::uint32_t label_message_id{};
    std::int32_t action_id{kUnownedNativeActionId};
    std::array<std::byte, kNativeDefinitionSize - 8> native_flags{};
};
static_assert(sizeof(KeyBindingDefinition) == kNativeDefinitionSize);

struct alignas(16) KeySettingStorage {
    std::array<std::byte, kKeySettingSize> bytes{};
};
static_assert(sizeof(KeySettingStorage) == kKeySettingSize);

struct BindingValue {
    std::array<std::uint32_t, 5> words{};
};
static_assert(sizeof(BindingValue) == 0x14);

struct InputToken {
    std::array<std::uint32_t, 3> words{};
};
static_assert(sizeof(InputToken) == 0x0C);

struct InputQueryRecord {
    std::int32_t physical_id{-1};
    std::uint8_t analog{};
    std::uint8_t digital_active{};
    std::uint16_t reserved{};
    float analog_value{};
};
static_assert(sizeof(InputQueryRecord) == 0x0C);

using BuildKeySettingListFn = void(__fastcall*)(void* list, void* config);
using ConstructKeySettingFn = void*(__fastcall*)(
    void* destination,
    std::uint32_t kind,
    const KeyBindingDefinition* definition);
using ConstructSpacerFn = void*(__fastcall*)(void* destination);
using AppendKeySettingFn = void(__fastcall*)(void* vector, const void* item);
using PollKeyCaptureFn = void*(__fastcall*)(
    void* item,
    void* result,
    void* config,
    void* cancel_input);
using ClearKeySettingFn = void(__fastcall*)(void* item, void* config);
using RefreshKeyConflictFn = void(__fastcall*)(void* item, void* config);
using WriteBindingValueFn = void(__fastcall*)(
    BindingValue* value,
    std::uint32_t kind,
    const InputToken* token);
using InputTokenPhysicalIdFn = std::int32_t(__fastcall*)(
    const InputToken* token);
using InputTokenAnalogFn = std::uint8_t(__fastcall*)(const InputToken* token);
using GetPlayerInputFn = void*(__fastcall*)(
    void* input_manager,
    std::uint32_t player_index);
using QueryInputStatesFn = void(__fastcall*)(
    void* player_input,
    InputQueryRecord* records,
    std::int32_t count);
using InputManagerUpdateFn = void(__fastcall*)(
    void* input_manager,
    void* frame_context);
using SoftwareKeyboardJobConstructFn = void*(__fastcall*)(
    void* self,
    void* parent,
    void* config,
    const void* resource_descriptor,
    const wchar_t* initial_text,
    std::uint8_t mode,
    void* completion_action);
using NativeTextEditorDestroyFn = void(__fastcall*)(void* self);
using TextInputDialogConstructFn = void*(__fastcall*)(
    void* self,
    void* scene_or_movie,
    void* editor_config,
    void* initial_text,
    void* completion_action);

struct NativeInputBindingAddresses {
    BuildKeySettingListFn build_key_setting_list{};
    ConstructKeySettingFn construct_key_setting{};
    ConstructSpacerFn construct_spacer{};
    AppendKeySettingFn append_key_setting{};
    PollKeyCaptureFn poll_key_capture{};
    ClearKeySettingFn clear_key_setting{};
    RefreshKeyConflictFn refresh_key_conflict{};
    WriteBindingValueFn write_binding_value{};
    InputTokenPhysicalIdFn input_token_physical_id{};
    InputTokenAnalogFn input_token_analog{};
    GetPlayerInputFn get_player_input{};
    QueryInputStatesFn query_input_states{};
    InputManagerUpdateFn input_manager_update{};
    SoftwareKeyboardJobConstructFn software_keyboard_construct{};
    NativeTextEditorDestroyFn software_keyboard_destroy{};
    TextInputDialogConstructFn text_input_dialog_construct{};
    NativeTextEditorDestroyFn text_input_dialog_destroy{};
    void** input_manager_slot{};
    void* key_config_dialog_vtable{};
    void* key_setting_list_vtable{};
    void* key_config_vtable{};
    void* software_keyboard_vtable{};
    void* text_input_dialog_vtable{};

    [[nodiscard]] bool complete() const noexcept {
        return build_key_setting_list && construct_key_setting &&
            construct_spacer && append_key_setting && poll_key_capture &&
            clear_key_setting && refresh_key_conflict &&
            write_binding_value && input_token_physical_id &&
            input_token_analog && get_player_input && query_input_states &&
            input_manager_update && software_keyboard_construct &&
            software_keyboard_destroy && text_input_dialog_construct &&
            text_input_dialog_destroy && input_manager_slot &&
            key_config_dialog_vtable && key_setting_list_vtable &&
            key_config_vtable && software_keyboard_vtable &&
            text_input_dialog_vtable;
    }
};

constexpr std::size_t kNativeInputFunctionCount = 17;

struct CapturedFunctionEntry {
    const void* address{};
    std::array<std::uint8_t, 16> bytes{};
};

using CapturedFunctionEntries =
    std::array<CapturedFunctionEntry, kNativeInputFunctionCount>;

struct VersionedActionInputs {
    ERUI_ActionInputs value{};
    std::uint32_t revision{};
};

// Multiple client threads may stage an assignment while the native capture
// and input-manager threads are live. A CAS-acquired odd revision serializes
// writers; readers never wait on a preempted writer and simply retry later.
class ActionInputsPublication {
public:
    [[nodiscard]] bool publish(
        const ERUI_ActionInputs& value,
        std::uint32_t& published_revision) noexcept {
        std::array<std::uint32_t, sizeof(ERUI_ActionInputs) /
            sizeof(std::uint32_t)> words{};
        static_assert(sizeof(words) == sizeof(value));
        std::memcpy(words.data(), &value, sizeof(value));

        for (unsigned attempt = 0; attempt < 64; ++attempt) {
            std::uint32_t revision =
                revision_.load(std::memory_order_acquire);
            if ((revision & 1u) != 0 ||
                !revision_.compare_exchange_weak(
                    revision,
                    revision + 1u,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                continue;
            }
            for (std::size_t index = 0; index < words.size(); ++index) {
                words_[index].store(words[index], std::memory_order_relaxed);
            }
            published_revision = revision == 0xFFFFFFFEu
                ? 2u
                : revision + 2u;
            revision_.store(published_revision, std::memory_order_release);
            return true;
        }
        published_revision = 0;
        return false;
    }

    // Atomically replaces a snapshot only when it is still the one the
    // caller inspected. Native capture/clear uses this compare-and-publish
    // operation so changing one device slot cannot overwrite a concurrent
    // complete assignment staged by a client thread.
    [[nodiscard]] bool publish_if_revision(
        std::uint32_t expected_revision,
        const ERUI_ActionInputs& value,
        std::uint32_t& published_revision) noexcept {
        if (expected_revision == 0 || (expected_revision & 1u) != 0) {
            published_revision = 0;
            return false;
        }

        std::array<std::uint32_t, sizeof(ERUI_ActionInputs) /
            sizeof(std::uint32_t)> words{};
        static_assert(sizeof(words) == sizeof(value));
        std::memcpy(words.data(), &value, sizeof(value));

        std::uint32_t revision = expected_revision;
        if (!revision_.compare_exchange_strong(
                revision,
                expected_revision + 1u,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            published_revision = 0;
            return false;
        }
        for (std::size_t index = 0; index < words.size(); ++index) {
            words_[index].store(words[index], std::memory_order_relaxed);
        }
        published_revision = expected_revision == 0xFFFFFFFEu
            ? 2u
            : expected_revision + 2u;
        revision_.store(published_revision, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_load(
        VersionedActionInputs& output) const noexcept {
        output.revision = revision_.load(std::memory_order_acquire);
        if ((output.revision & 1u) != 0) return false;
        std::array<std::uint32_t, sizeof(ERUI_ActionInputs) /
            sizeof(std::uint32_t)> words{};
        for (std::size_t index = 0; index < words.size(); ++index) {
            words[index] = words_[index].load(std::memory_order_relaxed);
        }
        const std::uint32_t confirmed =
            revision_.load(std::memory_order_acquire);
        if (confirmed != output.revision || (confirmed & 1u) != 0) {
            return false;
        }
        std::memcpy(&output.value, words.data(), sizeof(output.value));
        return true;
    }

    [[nodiscard]] std::uint32_t revision() const noexcept {
        return revision_.load(std::memory_order_acquire);
    }

private:
    std::atomic<std::uint32_t> revision_{2};
    std::array<std::atomic<std::uint32_t>,
        sizeof(ERUI_ActionInputs) / sizeof(std::uint32_t)> words_{};
};

struct NativeBinding {
    ERUI_InputActionHandle handle{};
    std::string provider_id{};
    std::string binding_id{};
    erui::TextId label_id{};
    erui::InputBindingAction action{};
    erui::InputAssignmentsAction assignments_changed{};
    ERUI_ActionInputs default_inputs{};
    ERUI_InputDevices supported_devices{};
    BindingValue value{};
    std::array<KeyBindingDefinition, kConfigModeCount> definitions{};
    RuntimeBindingPublication runtime_publication{};
    ActionInputsPublication desired_inputs{};
    std::atomic<std::uint32_t> applied_revision{2};
    RuntimeBindingEdgeTracker edge_tracker{};
};

struct NativeBindingSection {
    erui::TextId label_id{};
    std::array<KeyBindingDefinition, kConfigModeCount> definitions{};
    std::array<std::vector<NativeBinding*>, kConfigModeCount> bindings{};
};

struct OwnershipEntry {
    const KeyBindingDefinition* definition{};
    BindingValue* value{};
    NativeBinding* binding{};
    std::uint8_t mode{};
    std::uint8_t allowed_kind_mask{};
};

struct BindingEvent {
    NativeBinding* binding{};
    std::uint32_t revision{};
    RuntimeBindingDeviceMask devices{};
};

struct AssignmentEvent {
    NativeBinding* binding{};
    std::uint32_t revision{};
    ERUI_ActionInputs previous{};
    ERUI_ActionInputs current{};
    ERUI_AssignmentChangeReason reason{};
    ERUI_InputDevices changed_devices{};
};

template <typename Event>
class EventQueue {
public:
    [[nodiscard]] bool initialize(std::size_t capacity) {
        if (capacity < 2) return false;
        entries_.assign(capacity, {});
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] bool push(Event event) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = increment(head);
        if (next == tail_.load(std::memory_order_acquire)) return false;
        entries_[head] = event;
        head_.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool pop(Event& event) noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        event = entries_[tail];
        tail_.store(increment(tail), std::memory_order_release);
        return true;
    }

    void reset_unpublished() noexcept {
        entries_.clear();
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

private:
    [[nodiscard]] std::size_t increment(std::size_t value) const noexcept {
        ++value;
        return value == entries_.size() ? 0 : value;
    }

    std::vector<Event> entries_{};
    std::atomic<std::size_t> head_{};
    std::atomic<std::size_t> tail_{};
};

struct QueryMapping {
    std::uint32_t binding_index{};
    std::uint8_t slot{};
};

struct HandleEntry {
    ERUI_InputActionHandle handle{};
    NativeBinding* binding{};
};

SafetyHookInline g_build_hook{};
SafetyHookInline g_capture_poll_hook{};
SafetyHookInline g_clear_hook{};
SafetyHookInline g_conflict_hook{};
SafetyHookInline g_input_update_hook{};
SafetyHookInline g_software_keyboard_construct_hook{};
SafetyHookInline g_software_keyboard_destroy_hook{};
SafetyHookInline g_text_input_dialog_construct_hook{};
SafetyHookInline g_text_input_dialog_destroy_hook{};

std::atomic<BuildKeySettingListFn> g_original_build{};
std::atomic<PollKeyCaptureFn> g_original_capture_poll{};
std::atomic<ClearKeySettingFn> g_original_clear{};
std::atomic<RefreshKeyConflictFn> g_original_conflict{};
std::atomic<InputManagerUpdateFn> g_original_input_update{};
std::atomic<SoftwareKeyboardJobConstructFn>
    g_original_software_keyboard_construct{};
std::atomic<NativeTextEditorDestroyFn>
    g_original_software_keyboard_destroy{};
std::atomic<TextInputDialogConstructFn>
    g_original_text_input_dialog_construct{};
std::atomic<NativeTextEditorDestroyFn>
    g_original_text_input_dialog_destroy{};

NativeInputBindingAddresses g_addresses{};
NativeInputBindingAddresses g_captured_addresses{};
CapturedFunctionEntries g_captured_function_entries{};
HMODULE g_captured_allowed_owner{};
std::atomic_bool g_captured_addresses_ready{};
std::atomic_bool g_captured_addresses_approved{};
std::vector<std::unique_ptr<NativeBindingSection>> g_sections{};
std::vector<std::unique_ptr<NativeBinding>> g_bindings{};
std::vector<HandleEntry> g_handles{};
std::vector<OwnershipEntry> g_ownership{};
std::array<std::vector<KeySettingStorage>, kConfigModeCount> g_cell_scratch{};
std::vector<InputQueryRecord> g_query_records{};
std::vector<QueryMapping> g_query_mapping{};
std::vector<std::array<std::uint8_t, kRuntimeBindingSlotCount>>
    g_active_slots{};
EventQueue<BindingEvent> g_events{};
EventQueue<AssignmentEvent> g_assignment_events{};

std::array<std::atomic<void*>, kMaximumTrackedTextEditors>
    g_active_text_editors{};
std::array<std::atomic_bool, kConfigModeCount> g_mode_initialized{};
std::atomic_flag g_builder_busy = ATOMIC_FLAG_INIT;
std::atomic_bool g_prepared{};
std::atomic_bool g_installed{};
std::atomic_bool g_rows_published{};
std::atomic_bool g_builder_failed{};
std::atomic_bool g_builder_failure_reported{};
std::atomic<std::uint32_t> g_builder_failure_stage{};
std::atomic<std::uint32_t> g_builder_failure_mode{};
std::atomic<std::uint32_t> g_builder_failure_cell{};
std::atomic<std::uint32_t> g_builder_failure_exception{};
std::atomic<std::uint32_t> g_builder_failure_initial_count{};
std::atomic<std::uint32_t> g_builder_failure_initial_capacity{};
std::atomic<std::uint32_t> g_builder_failure_final_count{};
std::atomic<std::uint32_t> g_builder_failure_final_capacity{};
DefinitionScanTelemetry g_builder_failure_definition_scan{};
std::atomic_bool g_dispatch_enabled{};
std::atomic_bool g_runtime_failed{};
std::atomic_bool g_remap_capture_active{};
std::atomic_bool g_text_editor_tracking_ready{};
std::atomic_bool g_text_editor_tracking_failed{};
std::atomic<DWORD> g_input_thread{};
std::atomic<std::uint32_t> g_dropped_events{};
std::atomic<std::uint32_t> g_dropped_assignment_events{};
std::atomic<std::uint32_t> g_callback_faults{};

template <typename Value>
Value& field(void* object, std::size_t offset) noexcept {
    return *reinterpret_cast<Value*>(
        static_cast<std::byte*>(object) + offset);
}

template <typename Value>
const Value& field(const void* object, std::size_t offset) noexcept {
    return *reinterpret_cast<const Value*>(
        static_cast<const std::byte*>(object) + offset);
}

template <typename Function>
Function resolve_function(
    const ModuleView& game,
    std::uintptr_t rva,
    std::string_view pattern) noexcept {
    std::uint8_t* const address = game.at_rva(rva);
    return address && game.is_executable(address) &&
            game.matches(address, pattern)
        ? reinterpret_cast<Function>(address)
        : nullptr;
}

void log_failed_address_validation(
    const ModuleView& game,
    const char* name,
    std::uintptr_t rva) noexcept {
    std::array<std::uint8_t, 16> entry{};
    const std::uint8_t* const address = game.at_rva(rva);
    const bool in_image = address && game.contains(address, entry.size());
    const bool executable = address && game.is_executable(address);
    if (in_image) std::memcpy(entry.data(), address, entry.size());
    erui::detail::logf(
        erui::LogLevel::error,
        "Input binding address %-24s failed validation at RVA 0x%llX (inImage=%d executable=%d entry=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X)",
        name,
        static_cast<unsigned long long>(rva),
        in_image ? 1 : 0,
        executable ? 1 : 0,
        static_cast<unsigned>(entry[0]),
        static_cast<unsigned>(entry[1]),
        static_cast<unsigned>(entry[2]),
        static_cast<unsigned>(entry[3]),
        static_cast<unsigned>(entry[4]),
        static_cast<unsigned>(entry[5]),
        static_cast<unsigned>(entry[6]),
        static_cast<unsigned>(entry[7]),
        static_cast<unsigned>(entry[8]),
        static_cast<unsigned>(entry[9]),
        static_cast<unsigned>(entry[10]),
        static_cast<unsigned>(entry[11]),
        static_cast<unsigned>(entry[12]),
        static_cast<unsigned>(entry[13]),
        static_cast<unsigned>(entry[14]),
        static_cast<unsigned>(entry[15]));
}

void log_failed_data_validation(
    const ModuleView& game,
    const char* name,
    const void* address) noexcept {
    erui::detail::logf(
        erui::LogLevel::error,
        "Input binding data %-27s failed validation (address=%p inImage=%d)",
        name,
        address,
        address && game.contains(address, sizeof(void*)) ? 1 : 0);
}

bool resolve_native_addresses(
    NativeInputBindingAddresses& output,
    bool log_failure) noexcept {
    output = {};
    ModuleView game{};
    if (!game.initialize(GetModuleHandleW(nullptr))) {
        if (log_failure) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Input bindings unavailable: game image initialization failed");
        }
        return false;
    }

    const EldenRingBuild build = identify_elden_ring_build(
        game.timestamp(), game.image_size());
    const InputBindingsAddressProfile* const profile =
        find_input_bindings_address_profile(build);
    if (!profile) {
        if (log_failure) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Input bindings unavailable: unsupported game image timestamp=0x%08X size=0x%zX",
                static_cast<unsigned>(game.timestamp()),
                game.image_size());
        }
        return false;
    }

    const std::string_view build_name = elden_ring_build_name(build);
    erui::detail::logf(
        erui::LogLevel::info,
        "Input bindings native profile: Elden Ring %.*s timestamp=0x%08X size=0x%zX",
        static_cast<int>(build_name.size()),
        build_name.data(),
        static_cast<unsigned>(game.timestamp()),
        game.image_size());

    output.build_key_setting_list =
        resolve_function<BuildKeySettingListFn>(
            game, profile->build_key_setting_list_rva,
            kBuildKeySettingListPattern);
    output.construct_key_setting =
        resolve_function<ConstructKeySettingFn>(
            game, profile->construct_key_setting_rva,
            kConstructKeySettingPattern);
    output.construct_spacer = resolve_function<ConstructSpacerFn>(
        game, profile->construct_spacer_rva, kConstructSpacerPattern);
    output.append_key_setting = resolve_function<AppendKeySettingFn>(
        game, profile->append_key_setting_rva, kAppendKeySettingPattern);
    output.poll_key_capture = resolve_function<PollKeyCaptureFn>(
        game, profile->poll_key_capture_rva, kPollKeyCapturePattern);
    output.clear_key_setting = resolve_function<ClearKeySettingFn>(
        game, profile->clear_key_setting_rva, kClearKeySettingPattern);
    output.refresh_key_conflict = resolve_function<RefreshKeyConflictFn>(
        game, profile->refresh_key_conflict_rva,
        kRefreshKeyConflictPattern);
    output.write_binding_value = resolve_function<WriteBindingValueFn>(
        game, profile->write_binding_value_rva,
        kWriteBindingValuePattern);
    output.input_token_physical_id =
        resolve_function<InputTokenPhysicalIdFn>(
            game, profile->input_token_physical_id_rva,
            kInputTokenPhysicalIdPattern);
    output.input_token_analog = resolve_function<InputTokenAnalogFn>(
        game, profile->input_token_analog_rva,
        kInputTokenAnalogPattern);
    output.get_player_input = resolve_function<GetPlayerInputFn>(
        game, profile->get_player_input_rva, kGetPlayerInputPattern);
    output.query_input_states = resolve_function<QueryInputStatesFn>(
        game, profile->query_input_states_rva, kQueryInputStatesPattern);
    output.input_manager_update = resolve_function<InputManagerUpdateFn>(
        game, profile->input_manager_update_rva, kInputManagerUpdatePattern);
    output.software_keyboard_construct =
        resolve_function<SoftwareKeyboardJobConstructFn>(
            game,
            profile->software_keyboard_job_construct_rva,
            kSoftwareKeyboardJobConstructPattern);
    output.software_keyboard_destroy =
        resolve_function<NativeTextEditorDestroyFn>(
            game,
            profile->software_keyboard_job_destroy_rva,
            kSoftwareKeyboardJobDestroyPattern);
    output.text_input_dialog_construct =
        resolve_function<TextInputDialogConstructFn>(
            game,
            profile->text_input_dialog_construct_rva,
            kTextInputDialogConstructPattern);
    output.text_input_dialog_destroy =
        resolve_function<NativeTextEditorDestroyFn>(
            game,
            profile->text_input_dialog_destroy_rva,
            kTextInputDialogDestroyPattern);

    auto* const bounded_config_writer =
        game.at_rva(profile->write_menu_config_rva);
    output.input_manager_slot = reinterpret_cast<void**>(
        game.at_rva(profile->input_manager_slot_rva));
    output.key_config_dialog_vtable =
        game.at_rva(profile->key_config_dialog_vtable_rva);
    output.key_setting_list_vtable =
        game.at_rva(profile->key_setting_list_vtable_rva);
    output.key_config_vtable =
        game.at_rva(profile->key_config_vtable_rva);
    output.software_keyboard_vtable =
        game.at_rva(profile->software_keyboard_job_vtable_rva);
    output.text_input_dialog_vtable =
        game.at_rva(profile->text_input_dialog_vtable_rva);

    const bool data_ready =
        bounded_config_writer &&
        game.is_executable(bounded_config_writer) &&
        game.matches(bounded_config_writer, kWriteMenuConfigPattern) &&
        game.contains(output.input_manager_slot, sizeof(void*)) &&
        game.contains(output.key_config_dialog_vtable, sizeof(void*)) &&
        game.contains(output.key_setting_list_vtable, sizeof(void*)) &&
        game.contains(output.key_config_vtable, sizeof(void*)) &&
        game.contains(output.software_keyboard_vtable, sizeof(void*)) &&
        game.contains(output.text_input_dialog_vtable, sizeof(void*));
    if (!output.complete() || !data_ready) {
        if (log_failure) {
            const auto report_function = [&game](
                const void* function,
                const char* name,
                std::uintptr_t rva) noexcept {
                if (!function) log_failed_address_validation(game, name, rva);
            };
            report_function(
                reinterpret_cast<const void*>(output.build_key_setting_list),
                "build key-setting list",
                profile->build_key_setting_list_rva);
            report_function(
                reinterpret_cast<const void*>(output.construct_spacer),
                "construct spacer",
                profile->construct_spacer_rva);
            report_function(
                reinterpret_cast<const void*>(output.append_key_setting),
                "append key setting",
                profile->append_key_setting_rva);
            report_function(
                reinterpret_cast<const void*>(output.construct_key_setting),
                "construct key setting",
                profile->construct_key_setting_rva);
            report_function(
                reinterpret_cast<const void*>(output.poll_key_capture),
                "poll key capture",
                profile->poll_key_capture_rva);
            report_function(
                reinterpret_cast<const void*>(output.clear_key_setting),
                "clear key setting",
                profile->clear_key_setting_rva);
            report_function(
                reinterpret_cast<const void*>(output.refresh_key_conflict),
                "refresh key conflict",
                profile->refresh_key_conflict_rva);
            report_function(
                reinterpret_cast<const void*>(output.write_binding_value),
                "write binding value",
                profile->write_binding_value_rva);
            report_function(
                reinterpret_cast<const void*>(output.input_token_physical_id),
                "input token physical ID",
                profile->input_token_physical_id_rva);
            report_function(
                reinterpret_cast<const void*>(output.input_token_analog),
                "input token analog",
                profile->input_token_analog_rva);
            report_function(
                reinterpret_cast<const void*>(output.get_player_input),
                "get player input",
                profile->get_player_input_rva);
            report_function(
                reinterpret_cast<const void*>(output.query_input_states),
                "query input states",
                profile->query_input_states_rva);
            report_function(
                reinterpret_cast<const void*>(output.input_manager_update),
                "input manager update",
                profile->input_manager_update_rva);
            report_function(
                reinterpret_cast<const void*>(output.software_keyboard_construct),
                "software keyboard construct",
                profile->software_keyboard_job_construct_rva);
            report_function(
                reinterpret_cast<const void*>(output.software_keyboard_destroy),
                "software keyboard destroy",
                profile->software_keyboard_job_destroy_rva);
            report_function(
                reinterpret_cast<const void*>(output.text_input_dialog_construct),
                "text input dialog construct",
                profile->text_input_dialog_construct_rva);
            report_function(
                reinterpret_cast<const void*>(output.text_input_dialog_destroy),
                "text input dialog destroy",
                profile->text_input_dialog_destroy_rva);
            if (!bounded_config_writer ||
                !game.is_executable(bounded_config_writer) ||
                !game.matches(
                    bounded_config_writer, kWriteMenuConfigPattern)) {
                log_failed_address_validation(
                    game,
                    "bounded menu-config writer",
                    profile->write_menu_config_rva);
            }
            if (!game.contains(output.input_manager_slot, sizeof(void*))) {
                log_failed_data_validation(
                    game, "input-manager slot", output.input_manager_slot);
            }
            if (!game.contains(output.key_config_dialog_vtable, sizeof(void*))) {
                log_failed_data_validation(
                    game, "KeyConfig dialog vtable",
                    output.key_config_dialog_vtable);
            }
            if (!game.contains(output.key_setting_list_vtable, sizeof(void*))) {
                log_failed_data_validation(
                    game, "key-setting list vtable",
                    output.key_setting_list_vtable);
            }
            if (!game.contains(output.key_config_vtable, sizeof(void*))) {
                log_failed_data_validation(
                    game, "KeyConfig vtable", output.key_config_vtable);
            }
            if (!game.contains(output.software_keyboard_vtable, sizeof(void*))) {
                log_failed_data_validation(
                    game, "software-keyboard vtable",
                    output.software_keyboard_vtable);
            }
            if (!game.contains(output.text_input_dialog_vtable, sizeof(void*))) {
                log_failed_data_validation(
                    game, "text-input dialog vtable",
                    output.text_input_dialog_vtable);
            }
            erui::detail::logf(
                erui::LogLevel::error,
                "Input bindings unavailable: one or more exact native interfaces failed validation");
        }
        output = {};
        return false;
    }
    return true;
}

enum class CapturedEntryState : std::uint8_t {
    pristine,
    allowed_detour,
    invalid,
};

std::array<const void*, kNativeInputFunctionCount> native_function_entries(
    const NativeInputBindingAddresses& addresses) noexcept {
    return {
        reinterpret_cast<const void*>(addresses.build_key_setting_list),
        reinterpret_cast<const void*>(addresses.construct_key_setting),
        reinterpret_cast<const void*>(addresses.construct_spacer),
        reinterpret_cast<const void*>(addresses.append_key_setting),
        reinterpret_cast<const void*>(addresses.poll_key_capture),
        reinterpret_cast<const void*>(addresses.clear_key_setting),
        reinterpret_cast<const void*>(addresses.refresh_key_conflict),
        reinterpret_cast<const void*>(addresses.write_binding_value),
        reinterpret_cast<const void*>(addresses.input_token_physical_id),
        reinterpret_cast<const void*>(addresses.input_token_analog),
        reinterpret_cast<const void*>(addresses.get_player_input),
        reinterpret_cast<const void*>(addresses.query_input_states),
        reinterpret_cast<const void*>(addresses.input_manager_update),
        reinterpret_cast<const void*>(addresses.software_keyboard_construct),
        reinterpret_cast<const void*>(addresses.software_keyboard_destroy),
        reinterpret_cast<const void*>(addresses.text_input_dialog_construct),
        reinterpret_cast<const void*>(addresses.text_input_dialog_destroy),
    };
}

const void* absolute_indirect_jump_target(
    const ModuleView& game,
    const std::uint8_t* entry) noexcept {
    if (!entry || !game.contains(entry, 6) ||
        entry[0] != 0xFF || entry[1] != 0x25) {
        return nullptr;
    }

    std::int32_t displacement{};
    std::memcpy(&displacement, entry + 2, sizeof(displacement));
    const std::uintptr_t instruction_end =
        reinterpret_cast<std::uintptr_t>(entry) + 6u;
    std::uintptr_t slot_value{};
    if (displacement >= 0) {
        const auto distance = static_cast<std::uintptr_t>(displacement);
        if (instruction_end >
            (std::numeric_limits<std::uintptr_t>::max)() - distance) {
            return nullptr;
        }
        slot_value = instruction_end + distance;
    } else {
        const auto distance = static_cast<std::uintptr_t>(
            -static_cast<std::int64_t>(displacement));
        if (instruction_end < distance) return nullptr;
        slot_value = instruction_end - distance;
    }
    const auto* const slot = reinterpret_cast<const std::uint8_t*>(
        slot_value);
    if (!game.contains(slot, sizeof(void*))) return nullptr;

    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(slot, &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return nullptr;
    }
    const auto region_end = reinterpret_cast<std::uintptr_t>(
        memory.BaseAddress) + memory.RegionSize;
    if (slot_value > region_end ||
        sizeof(void*) > region_end - slot_value) {
        return nullptr;
    }

    const void* target{};
#if defined(_MSC_VER)
    __try {
#endif
    std::memcpy(&target, slot, sizeof(target));
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
#endif
    return target;
}

CapturedEntryState classify_captured_entry(
    const ModuleView& game,
    const ModuleView& allowed_owner,
    const void* captured,
    const CapturedFunctionEntry& snapshot,
    std::uintptr_t expected_rva,
    std::string_view pristine_pattern,
    bool allow_owned_detour) noexcept {
    const auto* const expected = game.at_rva(expected_rva);
    if (!captured || captured != expected || captured != snapshot.address ||
        !game.contains(expected, snapshot.bytes.size()) ||
        !game.is_executable(expected)) {
        return CapturedEntryState::invalid;
    }
    if (std::memcmp(
            expected, snapshot.bytes.data(), snapshot.bytes.size()) == 0 &&
        game.matches(expected, pristine_pattern)) {
        return CapturedEntryState::pristine;
    }
    if (!allow_owned_detour) return CapturedEntryState::invalid;

    const void* const destination = absolute_indirect_jump_target(
        game, expected);
    return destination && allowed_owner.contains(destination) &&
            allowed_owner.is_executable(destination)
        ? CapturedEntryState::allowed_detour
        : CapturedEntryState::invalid;
}

const char* captured_entry_state_name(CapturedEntryState state) noexcept {
    switch (state) {
    case CapturedEntryState::pristine:
        return "pristine";
    case CapturedEntryState::allowed_detour:
        return "owned-detour";
    case CapturedEntryState::invalid:
        return "invalid";
    }
    return "invalid";
}

bool validate_captured_native_addresses(
    const NativeInputBindingAddresses& captured,
    HMODULE allowed_detour_module) noexcept {
    ModuleView game{};
    ModuleView allowed_owner{};
    if (!captured.complete() ||
        !game.initialize(GetModuleHandleW(nullptr)) ||
        !allowed_owner.initialize(allowed_detour_module)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Solid Uncapper input chaining: captured addresses or module images are unavailable");
        return false;
    }

    const EldenRingBuild build = identify_elden_ring_build(
        game.timestamp(), game.image_size());
    const InputBindingsAddressProfile* const profile =
        find_input_bindings_address_profile(build);
    if (!profile) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Solid Uncapper input chaining: current game build has no exact input profile");
        return false;
    }

    const auto entries = native_function_entries(captured);
    const std::array<std::uintptr_t, kNativeInputFunctionCount> rvas{
        profile->build_key_setting_list_rva,
        profile->construct_key_setting_rva,
        profile->construct_spacer_rva,
        profile->append_key_setting_rva,
        profile->poll_key_capture_rva,
        profile->clear_key_setting_rva,
        profile->refresh_key_conflict_rva,
        profile->write_binding_value_rva,
        profile->input_token_physical_id_rva,
        profile->input_token_analog_rva,
        profile->get_player_input_rva,
        profile->query_input_states_rva,
        profile->input_manager_update_rva,
        profile->software_keyboard_job_construct_rva,
        profile->software_keyboard_job_destroy_rva,
        profile->text_input_dialog_construct_rva,
        profile->text_input_dialog_destroy_rva,
    };
    const std::array<std::string_view, kNativeInputFunctionCount> patterns{
        kBuildKeySettingListPattern,
        kConstructKeySettingPattern,
        kConstructSpacerPattern,
        kAppendKeySettingPattern,
        kPollKeyCapturePattern,
        kClearKeySettingPattern,
        kRefreshKeyConflictPattern,
        kWriteBindingValuePattern,
        kInputTokenPhysicalIdPattern,
        kInputTokenAnalogPattern,
        kGetPlayerInputPattern,
        kQueryInputStatesPattern,
        kInputManagerUpdatePattern,
        kSoftwareKeyboardJobConstructPattern,
        kSoftwareKeyboardJobDestroyPattern,
        kTextInputDialogConstructPattern,
        kTextInputDialogDestroyPattern,
    };
    const std::array<const char*, kNativeInputFunctionCount> names{
        "build key-setting list",
        "construct key setting",
        "construct spacer",
        "append key setting",
        "poll key capture",
        "clear key setting",
        "refresh key conflict",
        "write binding value",
        "input token physical ID",
        "input token analog",
        "get player input",
        "query input states",
        "input manager update",
        "software keyboard construct",
        "software keyboard destroy",
        "text input dialog construct",
        "text input dialog destroy",
    };
    constexpr std::size_t kClearEntry = 5;
    constexpr std::size_t kWriteEntry = 7;
    std::array<CapturedEntryState, kNativeInputFunctionCount> states{};
    bool functions_ready = true;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const bool allowed_overlap =
            index == kClearEntry || index == kWriteEntry;
        states[index] = classify_captured_entry(
            game,
            allowed_owner,
            entries[index],
            g_captured_function_entries[index],
            rvas[index],
            patterns[index],
            allowed_overlap);
        if (states[index] == CapturedEntryState::invalid) {
            functions_ready = false;
            erui::detail::logf(
                erui::LogLevel::error,
                "Solid Uncapper input chaining: %s entry changed unexpectedly at RVA 0x%llX",
                names[index],
                static_cast<unsigned long long>(rvas[index]));
        }
    }
    const CapturedEntryState clear_state = states[kClearEntry];
    const CapturedEntryState write_state = states[kWriteEntry];

    const auto* const bounded_config_writer =
        game.at_rva(profile->write_menu_config_rva);
    const bool data_ready =
        bounded_config_writer &&
        game.is_executable(bounded_config_writer) &&
        game.matches(bounded_config_writer, kWriteMenuConfigPattern) &&
        captured.input_manager_slot == reinterpret_cast<void**>(
            game.at_rva(profile->input_manager_slot_rva)) &&
        captured.key_config_dialog_vtable ==
            game.at_rva(profile->key_config_dialog_vtable_rva) &&
        captured.key_setting_list_vtable ==
            game.at_rva(profile->key_setting_list_vtable_rva) &&
        captured.key_config_vtable ==
            game.at_rva(profile->key_config_vtable_rva) &&
        captured.software_keyboard_vtable ==
            game.at_rva(profile->software_keyboard_job_vtable_rva) &&
        captured.text_input_dialog_vtable ==
            game.at_rva(profile->text_input_dialog_vtable_rva) &&
        game.contains(captured.input_manager_slot, sizeof(void*)) &&
        game.contains(captured.key_config_dialog_vtable, sizeof(void*)) &&
        game.contains(captured.key_setting_list_vtable, sizeof(void*)) &&
        game.contains(captured.key_config_vtable, sizeof(void*)) &&
        game.contains(captured.software_keyboard_vtable, sizeof(void*)) &&
        game.contains(captured.text_input_dialog_vtable, sizeof(void*));

    erui::detail::logf(
        functions_ready && data_ready
            ? erui::LogLevel::info
            : erui::LogLevel::error,
        "Solid Uncapper input chaining: clear=%s write=%s allFunctions=%s data=%s",
        captured_entry_state_name(clear_state),
        captured_entry_state_name(write_state),
        functions_ready ? "valid" : "invalid",
        data_ready ? "valid" : "invalid");
    return functions_ready && data_ready;
}

std::string binding_identity(
    std::string_view provider_id,
    std::string_view binding_id) {
    std::string result{};
    result.reserve(provider_id.size() + binding_id.size() + 1);
    result.append(provider_id);
    result.push_back('\0');
    result.append(binding_id);
    return result;
}

void flag_runtime_failure() noexcept {
    g_runtime_failed.store(true, std::memory_order_release);
}

bool claim_input_mutation_thread() noexcept {
    const DWORD thread = GetCurrentThreadId();
    DWORD expected = 0;
    if (g_input_thread.compare_exchange_strong(
            expected,
            thread,
            std::memory_order_acq_rel,
            std::memory_order_acquire) ||
        expected == thread) {
        return true;
    }
    // The native BindingValue is retained by live game UI objects and is not
    // atomic. Refuse to mutate it from a second thread instead of introducing
    // a data race with the input-manager update hook.
    flag_runtime_failure();
    return false;
}

void fail_text_editor_tracking() noexcept {
    g_text_editor_tracking_failed.store(true, std::memory_order_release);
    flag_runtime_failure();
}

bool object_has_vtable(void* object, void* expected_vtable) noexcept {
    if (!object || !expected_vtable) return false;
#if defined(_MSC_VER)
    __try {
#endif
        return field<void*>(object, 0) == expected_vtable;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool track_text_editor(void* object) noexcept {
    if (!object) {
        fail_text_editor_tracking();
        return false;
    }
    for (auto& slot : g_active_text_editors) {
        void* expected = nullptr;
        if (slot.compare_exchange_strong(
                expected,
                object,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return true;
        }
        if (expected == object) {
            fail_text_editor_tracking();
            return false;
        }
    }
    fail_text_editor_tracking();
    return false;
}

bool untrack_text_editor(void* object) noexcept {
    if (!object) return false;
    for (auto& slot : g_active_text_editors) {
        void* expected = object;
        if (slot.compare_exchange_strong(
                expected,
                nullptr,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

bool native_text_editor_active() noexcept {
    if (!g_text_editor_tracking_ready.load(std::memory_order_acquire) ||
        g_text_editor_tracking_failed.load(std::memory_order_acquire)) {
        return true;
    }
    for (const auto& slot : g_active_text_editors) {
        if (slot.load(std::memory_order_acquire)) return true;
    }
    return false;
}

void* __fastcall software_keyboard_construct_detour(
    void* self,
    void* parent,
    void* config,
    const void* resource_descriptor,
    const wchar_t* initial_text,
    std::uint8_t mode,
    void* completion_action) noexcept {
    const SoftwareKeyboardJobConstructFn original =
        g_original_software_keyboard_construct.load(
            std::memory_order_acquire);
    if (!original) {
        fail_text_editor_tracking();
        return self;
    }
    void* const result = original(
        self,
        parent,
        config,
        resource_descriptor,
        initial_text,
        mode,
        completion_action);
    if (result != self ||
        !object_has_vtable(self, g_addresses.software_keyboard_vtable)) {
        fail_text_editor_tracking();
    } else {
        track_text_editor(self);
    }
    return result;
}

void __fastcall software_keyboard_destroy_detour(void* self) noexcept {
    const NativeTextEditorDestroyFn original =
        g_original_software_keyboard_destroy.load(
            std::memory_order_acquire);
    if (!original) {
        fail_text_editor_tracking();
        return;
    }
    if (!object_has_vtable(self, g_addresses.software_keyboard_vtable)) {
        fail_text_editor_tracking();
    }
    const bool tracking_ready =
        g_text_editor_tracking_ready.load(std::memory_order_acquire);
    // Keep the editor visible to the input thread through all native cleanup.
    original(self);
    if (!untrack_text_editor(self) && tracking_ready) {
        fail_text_editor_tracking();
    }
}

void* __fastcall text_input_dialog_construct_detour(
    void* self,
    void* scene_or_movie,
    void* editor_config,
    void* initial_text,
    void* completion_action) noexcept {
    const TextInputDialogConstructFn original =
        g_original_text_input_dialog_construct.load(
            std::memory_order_acquire);
    if (!original) {
        fail_text_editor_tracking();
        return self;
    }
    void* const result = original(
        self,
        scene_or_movie,
        editor_config,
        initial_text,
        completion_action);
    if (result != self ||
        !object_has_vtable(self, g_addresses.text_input_dialog_vtable)) {
        fail_text_editor_tracking();
    } else {
        track_text_editor(self);
    }
    return result;
}

void __fastcall text_input_dialog_destroy_detour(void* self) noexcept {
    const NativeTextEditorDestroyFn original =
        g_original_text_input_dialog_destroy.load(
            std::memory_order_acquire);
    if (!original) {
        fail_text_editor_tracking();
        return;
    }
    if (!object_has_vtable(self, g_addresses.text_input_dialog_vtable)) {
        fail_text_editor_tracking();
    }
    const bool tracking_ready =
        g_text_editor_tracking_ready.load(std::memory_order_acquire);
    original(self);
    if (!untrack_text_editor(self) && tracking_ready) {
        fail_text_editor_tracking();
    }
}

bool live_binding_list(
    void* list,
    void* config,
    std::uint8_t& mode) noexcept {
    if (!list || !config ||
        static_cast<std::byte*>(list) + kListToConfigOffset != config) {
        return false;
    }
    const auto* const dialog =
        static_cast<const std::byte*>(list) - kDialogToListOffset;
    mode = field<std::uint8_t>(config, kConfigModeOffset);
    return *reinterpret_cast<void* const*>(dialog) ==
            g_addresses.key_config_dialog_vtable &&
        *reinterpret_cast<void* const*>(list) ==
            g_addresses.key_setting_list_vtable &&
        *reinterpret_cast<void* const*>(config) ==
            g_addresses.key_config_vtable &&
        mode <= kKeyboardMouseConfigMode &&
        field<std::uint8_t>(dialog, kDialogModeOffset) == mode;
}

bool semantic_token_for_slot(
    const ERUI_ActionInputs& inputs,
    std::size_t slot,
    erui::detail::NativeInputToken& token) noexcept {
    switch (slot) {
    case kControllerKind:
        return inputs.controller.state == ERUI_INPUT_SLOT_BOUND &&
            erui::detail::controller_to_native_token(
                inputs.controller.input, token);
    case kKeyboardKind:
        return inputs.keyboard.state == ERUI_INPUT_SLOT_BOUND &&
            erui::detail::keyboard_to_native_token(
                inputs.keyboard.input, token);
    case kMouseKind:
        return inputs.mouse.state == ERUI_INPUT_SLOT_BOUND &&
            erui::detail::mouse_to_native_token(inputs.mouse.input, token);
    default:
        return false;
    }
}

bool encode_binding_value(
    const ERUI_ActionInputs& inputs,
    BindingValue& output) noexcept {
    if (!erui::detail::valid_action_default_inputs(inputs) ||
        !g_addresses.write_binding_value) {
        return false;
    }
    BindingValue encoded{};
    encoded.words = kEmptyBindingValue;
    bool valid = true;
#if defined(_MSC_VER)
    __try {
#endif
        for (std::size_t slot = 0;
             slot < kRuntimeBindingSlotCount;
             ++slot) {
            erui::detail::NativeInputToken semantic_token{};
            if (!semantic_token_for_slot(inputs, slot, semantic_token)) {
                continue;
            }
            const InputToken token{{
                semantic_token.code,
                semantic_token.kind,
                semantic_token.auxiliary}};
            g_addresses.write_binding_value(
                &encoded, static_cast<std::uint32_t>(slot), &token);
        }
        valid = encoded.words[2] == 0 && encoded.words[4] == 0;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        valid = false;
    }
#endif
    if (valid) output = encoded;
    return valid;
}

bool build_runtime_snapshot(
    const ERUI_ActionInputs& inputs,
    RuntimeBindingSnapshot& output) noexcept {
    RuntimeBindingSnapshot runtime{};
    bool valid = true;
#if defined(_MSC_VER)
    __try {
#endif
        for (std::size_t slot = 0;
             slot < kRuntimeBindingSlotCount;
             ++slot) {
            erui::detail::NativeInputToken semantic_token{};
            if (!semantic_token_for_slot(inputs, slot, semantic_token)) {
                continue;
            }
            const InputToken token{{
                semantic_token.code,
                semantic_token.kind,
                semantic_token.auxiliary}};
            const std::int32_t physical_id =
                g_addresses.input_token_physical_id(&token);
            if (physical_id == -1) {
                valid = false;
                break;
            }
            runtime.slots[slot].physical_id = physical_id;
            runtime.slots[slot].analog =
                g_addresses.input_token_analog(&token) != 0 ? 1u : 0u;
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        valid = false;
        runtime = {};
    }
#endif
    if (valid) output = runtime;
    return valid;
}

bool apply_action_inputs(
    NativeBinding& binding,
    const ERUI_ActionInputs& inputs,
    std::uint32_t revision) noexcept {
    BindingValue value{};
    RuntimeBindingSnapshot runtime{};
    if (!encode_binding_value(inputs, value) ||
        !build_runtime_snapshot(inputs, runtime)) {
        flag_runtime_failure();
        return false;
    }
    binding.value = value;
    binding.runtime_publication.publish(runtime);
    binding.applied_revision.store(revision, std::memory_order_release);
    return true;
}

bool write_action_inputs_value(
    NativeBinding& binding,
    const ERUI_ActionInputs& inputs) noexcept {
    BindingValue value{};
    if (!encode_binding_value(inputs, value)) {
        flag_runtime_failure();
        return false;
    }
    binding.value = value;
    return true;
}

bool assign_captured_token(
    ERUI_ActionInputs& inputs,
    std::uint32_t kind,
    const InputToken& token) noexcept {
    const erui::detail::NativeInputToken semantic_token{
        token.words[0], token.words[1], token.words[2]};
    switch (kind) {
    case kControllerKind: {
        if (inputs.controller.state == ERUI_INPUT_SLOT_ABSENT) return false;
        ERUI_ControllerButton value{};
        if (!erui::detail::controller_from_native_token(
                semantic_token, value)) {
            return false;
        }
        inputs.controller = {ERUI_INPUT_SLOT_BOUND, value};
        return true;
    }
    case kKeyboardKind: {
        if (inputs.keyboard.state == ERUI_INPUT_SLOT_ABSENT) return false;
        ERUI_KeyboardKey value{};
        if (!erui::detail::keyboard_from_native_token(
                semantic_token, value)) {
            return false;
        }
        inputs.keyboard = {ERUI_INPUT_SLOT_BOUND, value};
        return true;
    }
    case kMouseKind: {
        if (inputs.mouse.state == ERUI_INPUT_SLOT_ABSENT) return false;
        ERUI_MouseButton value{};
        if (!erui::detail::mouse_from_native_token(
                semantic_token, value)) {
            return false;
        }
        inputs.mouse = {ERUI_INPUT_SLOT_BOUND, value};
        return true;
    }
    default:
        return false;
    }
}

bool clear_input_slot(
    ERUI_ActionInputs& inputs,
    std::uint32_t kind) noexcept {
    switch (kind) {
    case kControllerKind:
        if (inputs.controller.state == ERUI_INPUT_SLOT_ABSENT) return false;
        inputs.controller = {
            ERUI_INPUT_SLOT_UNBOUND, ERUI_CONTROLLER_BUTTON_INVALID};
        return true;
    case kKeyboardKind:
        if (inputs.keyboard.state == ERUI_INPUT_SLOT_ABSENT) return false;
        inputs.keyboard = {
            ERUI_INPUT_SLOT_UNBOUND, ERUI_KEYBOARD_KEY_INVALID};
        return true;
    case kMouseKind:
        if (inputs.mouse.state == ERUI_INPUT_SLOT_ABSENT) return false;
        inputs.mouse = {
            ERUI_INPUT_SLOT_UNBOUND, ERUI_MOUSE_BUTTON_INVALID};
        return true;
    default:
        return false;
    }
}

bool initialize_mode_definitions(
    void* list,
    std::uint8_t mode,
    DefinitionScanTelemetry* telemetry) noexcept {
    if (mode >= kConfigModeCount) return false;
    if (g_mode_initialized[mode].load(std::memory_order_acquire)) {
        return true;
    }

    auto* const begin = field<std::byte*>(list, kListBeginOffset);
    auto* const end = field<std::byte*>(list, kListEndOffset);
    if (!begin || !end || end < begin ||
        static_cast<std::size_t>(end - begin) % kKeySettingSize != 0) {
        return false;
    }
    const std::size_t cell_count =
        static_cast<std::size_t>(end - begin) / kKeySettingSize;
    const bool controller_mode = mode == kControllerConfigMode;
    const KeyBindingDefinition* category_template = nullptr;
    const KeyBindingDefinition* action_template = nullptr;

    for (std::size_t cell = 0; cell + 1 < cell_count; cell += 2) {
        const std::byte* const primary =
            begin + cell * kKeySettingSize;
        const std::byte* const secondary = primary + kKeySettingSize;
        const std::uint32_t observed_primary_kind =
            field<std::uint32_t>(primary, kItemKindOffset);
        const std::uint32_t observed_secondary_kind =
            field<std::uint32_t>(secondary, kItemKindOffset);
        if (telemetry) {
            if (telemetry->pairs_total == 0) {
                telemetry->first_pair_kinds =
                    (observed_primary_kind & 0xFFFFu) |
                    ((observed_secondary_kind & 0xFFFFu) << 16u);
            }
            ++telemetry->pairs_total;
        }
        // The spacer constructor always emits kind 1. Consequently a
        // keyboard/mouse category is (keyboard, spacer) == (1, 1), while a
        // real action is (keyboard, mouse) == (1, 2). Controller mode uses
        // (controller, spacer) == (0, 1) for both categories and actions.
        const bool category_kind_pair = is_category_kind_pair(
            controller_mode,
            observed_primary_kind,
            observed_secondary_kind);
        const bool action_kind_pair = is_action_kind_pair(
            controller_mode,
            observed_primary_kind,
            observed_secondary_kind);
        if (!category_kind_pair && !action_kind_pair) {
            continue;
        }
        if (telemetry) ++telemetry->kind_pairs;
        const auto* const primary_definition =
            field<const KeyBindingDefinition*>(
                primary, kItemDefinitionOffset);
        const auto* const secondary_definition =
            field<const KeyBindingDefinition*>(
                secondary, kItemDefinitionOffset);
        void* const primary_binding =
            field<void*>(primary, kItemBindingOffset);
        void* const secondary_binding =
            field<void*>(secondary, kItemBindingOffset);
        const bool secondary_spacer =
            secondary_definition == nullptr &&
            secondary_binding == nullptr;
        const bool shared_secondary =
            secondary_definition == primary_definition &&
            secondary_binding == primary_binding;

        if (telemetry && category_kind_pair && primary_definition &&
            primary_definition->action_id == kUnownedNativeActionId) {
            const bool first = telemetry->category_sentinels == 0;
            ++telemetry->category_sentinels;
            if (!primary_binding) {
                ++telemetry->category_primary_null;
                if (secondary_spacer) ++telemetry->category_spacers;
            }
            if (first) {
                // Bits: primary definition/binding, secondary
                // definition/binding, shared definition/binding,
                // secondary spacer, fully shared secondary.
                telemetry->first_category_shape =
                    (primary_definition ? 1u << 0u : 0u) |
                    (primary_binding ? 1u << 1u : 0u) |
                    (secondary_definition ? 1u << 2u : 0u) |
                    (secondary_binding ? 1u << 3u : 0u) |
                    (secondary_definition == primary_definition
                         ? 1u << 4u
                         : 0u) |
                    (secondary_binding == primary_binding
                         ? 1u << 5u
                         : 0u) |
                    (secondary_spacer ? 1u << 6u : 0u) |
                    (shared_secondary ? 1u << 7u : 0u);
                std::memcpy(
                    &telemetry->first_category_flags_low,
                    primary_definition->native_flags.data(),
                    sizeof(telemetry->first_category_flags_low));
                std::memcpy(
                    &telemetry->first_category_flags_high,
                    primary_definition->native_flags.data() +
                        sizeof(telemetry->first_category_flags_low),
                    sizeof(telemetry->first_category_flags_high));
            }
        }

        const bool action_id_in_range = action_kind_pair &&
            primary_definition && primary_definition->action_id >= 0 &&
            primary_definition->action_id <= 0x35;
        if (telemetry && action_id_in_range) {
            const bool first = telemetry->action_ids_in_range == 0;
            ++telemetry->action_ids_in_range;
            if (primary_binding) {
                ++telemetry->action_primary_nonnull;
                const bool secondary_shape =
                    controller_mode ? secondary_spacer : shared_secondary;
                if (secondary_shape) {
                    ++telemetry->action_secondary_shape;
                    const bool flags_match = controller_mode
                        ? primary_definition->native_flags[1] !=
                                std::byte{0} &&
                            primary_definition->native_flags[8] !=
                                std::byte{0}
                        : primary_definition->native_flags[9] !=
                            std::byte{0};
                    if (flags_match) ++telemetry->action_flags;
                }
            }
            if (first) {
                telemetry->first_action_id =
                    static_cast<std::uint32_t>(
                        primary_definition->action_id);
                telemetry->first_action_shape =
                    (primary_definition ? 1u << 0u : 0u) |
                    (primary_binding ? 1u << 1u : 0u) |
                    (secondary_definition ? 1u << 2u : 0u) |
                    (secondary_binding ? 1u << 3u : 0u) |
                    (secondary_definition == primary_definition
                         ? 1u << 4u
                         : 0u) |
                    (secondary_binding == primary_binding
                         ? 1u << 5u
                         : 0u) |
                    (secondary_spacer ? 1u << 6u : 0u) |
                    (shared_secondary ? 1u << 7u : 0u);
                std::memcpy(
                    &telemetry->first_action_flags_low,
                    primary_definition->native_flags.data(),
                    sizeof(telemetry->first_action_flags_low));
                std::memcpy(
                    &telemetry->first_action_flags_high,
                    primary_definition->native_flags.data() +
                        sizeof(telemetry->first_action_flags_low),
                    sizeof(telemetry->first_action_flags_high));
            }
        }

        if (!category_template && category_kind_pair && primary_definition &&
            primary_definition->action_id == kUnownedNativeActionId &&
            !primary_binding && secondary_spacer) {
            category_template = primary_definition;
        }
        if (!action_template && action_kind_pair && primary_definition &&
            primary_binding &&
            primary_definition->action_id >= 0 &&
            primary_definition->action_id <= 0x35 &&
            (controller_mode ? secondary_spacer : shared_secondary) &&
            (controller_mode
                 ? primary_definition->native_flags[1] != std::byte{0} &&
                     primary_definition->native_flags[8] != std::byte{0}
                 : primary_definition->native_flags[9] != std::byte{0})) {
            action_template = primary_definition;
        }
        if (category_template && action_template) break;
    }
    if (!category_template || !action_template) return false;

    for (const auto& section : g_sections) {
        KeyBindingDefinition& definition = section->definitions[mode];
        std::memcpy(
            &definition, category_template, sizeof(definition));
        definition.label_message_id = section->label_id;
        definition.action_id = kUnownedNativeActionId;
    }
    for (const auto& binding : g_bindings) {
        KeyBindingDefinition& definition = binding->definitions[mode];
        std::memcpy(&definition, action_template, sizeof(definition));
        definition.label_message_id = binding->label_id;
        definition.action_id = kUnownedNativeActionId;
        definition.native_flags[0] = std::byte{1}; // native Clear
        if (controller_mode) {
            const bool supported =
                (binding->supported_devices &
                    ERUI_INPUT_DEVICE_CONTROLLER) != 0;
            definition.native_flags[1] =
                supported ? std::byte{1} : std::byte{0};
            definition.native_flags[8] =
                supported ? std::byte{1} : std::byte{0};
        } else {
            // The native keyboard cell has no independently discovered
            // disabled presentation bit. The ownership guard below still
            // rejects editing it for a mouse-only action. Mouse has an exact
            // enable bit and can be presented disabled correctly.
            definition.native_flags[2] =
                (binding->supported_devices & ERUI_INPUT_DEVICE_MOUSE) != 0
                ? std::byte{1}
                : std::byte{0};
            definition.native_flags[9] = std::byte{1};
        }
    }
    g_mode_initialized[mode].store(true, std::memory_order_release);
    return true;
}

bool validate_constructed_cell(
    const KeySettingStorage& cell,
    std::uint32_t kind,
    const KeyBindingDefinition* definition,
    const BindingValue* value) noexcept {
    return field<std::uint32_t>(
               cell.bytes.data(), kItemKindOffset) == kind &&
        field<const KeyBindingDefinition*>(
            cell.bytes.data(), kItemDefinitionOffset) == definition &&
        field<const void*>(
            cell.bytes.data(), kItemBindingOffset) == value;
}

bool build_scratch_cells(std::uint8_t mode) noexcept {
    if (mode >= kConfigModeCount) return false;
    const bool controller_mode = mode == kControllerConfigMode;
    const std::uint32_t primary_kind =
        controller_mode ? kControllerKind : kKeyboardKind;
    std::vector<KeySettingStorage>& scratch = g_cell_scratch[mode];
    std::size_t cell = 0;
    for (const auto& section : g_sections) {
        if (section->bindings[mode].empty()) continue;
        KeySettingStorage& category = scratch[cell++];
        KeySettingStorage& category_spacer = scratch[cell++];
        category = {};
        category_spacer = {};
        g_addresses.construct_key_setting(
            category.bytes.data(),
            primary_kind,
            &section->definitions[mode]);
        g_addresses.construct_spacer(category_spacer.bytes.data());
        if (!validate_constructed_cell(
                category,
                primary_kind,
                &section->definitions[mode],
                nullptr) ||
            !validate_constructed_cell(
                category_spacer,
                kKeyboardKind,
                nullptr,
                nullptr)) {
            return false;
        }

        for (NativeBinding* const binding : section->bindings[mode]) {
            KeySettingStorage& primary = scratch[cell++];
            KeySettingStorage& secondary = scratch[cell++];
            primary = {};
            secondary = {};
            g_addresses.construct_key_setting(
                primary.bytes.data(),
                primary_kind,
                &binding->definitions[mode]);
            field<void*>(primary.bytes.data(), kItemBindingOffset) =
                &binding->value;
            field<std::uint16_t>(
                primary.bytes.data(), kItemConflictOffset) = 0;
            if (controller_mode) {
                g_addresses.construct_spacer(secondary.bytes.data());
            } else {
                g_addresses.construct_key_setting(
                    secondary.bytes.data(),
                    kMouseKind,
                    &binding->definitions[mode]);
                field<void*>(secondary.bytes.data(), kItemBindingOffset) =
                    &binding->value;
                field<std::uint16_t>(
                    secondary.bytes.data(), kItemConflictOffset) = 0;
            }
            if (!validate_constructed_cell(
                    primary,
                    primary_kind,
                    &binding->definitions[mode],
                    &binding->value) ||
                !validate_constructed_cell(
                    secondary,
                    controller_mode ? kKeyboardKind : kMouseKind,
                    controller_mode ? nullptr : &binding->definitions[mode],
                    controller_mode ? nullptr : &binding->value)) {
                return false;
            }
        }
    }
    return cell == scratch.size();
}

bool validate_appended_tail(
    void* list,
    const std::vector<KeySettingStorage>& scratch) noexcept {
    auto* const begin = field<std::byte*>(list, kListBeginOffset);
    auto* const end = field<std::byte*>(list, kListEndOffset);
    const std::size_t byte_count =
        scratch.size() * kKeySettingSize;
    if (!begin || !end || end < begin ||
        static_cast<std::size_t>(end - begin) < byte_count) {
        return false;
    }
    const std::byte* const tail = end - byte_count;
    for (std::size_t index = 0;
         index < scratch.size();
         ++index) {
        const void* const actual = tail + index * kKeySettingSize;
        const void* const expected = scratch[index].bytes.data();
        if (field<std::uint32_t>(actual, kItemKindOffset) !=
                field<std::uint32_t>(expected, kItemKindOffset) ||
            field<const void*>(actual, kItemDefinitionOffset) !=
                field<const void*>(expected, kItemDefinitionOffset) ||
            field<const void*>(actual, kItemBindingOffset) !=
                field<const void*>(expected, kItemBindingOffset)) {
            return false;
        }
    }
    return true;
}

void append_compiled_bindings(void* list, void* config) noexcept {
    if (!list || !config ||
        g_builder_failed.load(std::memory_order_acquire) ||
        !g_prepared.load(std::memory_order_acquire) ||
        g_builder_busy.test_and_set(std::memory_order_acquire)) {
        return;
    }

    bool targeted = false;
    bool accepted = false;
    std::uint32_t failure_stage =
        static_cast<std::uint32_t>(BindingBuilderStage::live_list);
    std::uint32_t failure_mode = 0xFFFFFFFFu;
    std::uint32_t failure_cell = 0xFFFFFFFFu;
    std::uint32_t seh_code = 0;
    std::uint32_t initial_count = 0xFFFFFFFFu;
    std::uint32_t initial_capacity = 0xFFFFFFFFu;
    std::uint32_t final_count = 0xFFFFFFFFu;
    std::uint32_t final_capacity = 0xFFFFFFFFu;
    DefinitionScanTelemetry definition_scan{};
#if defined(_MSC_VER)
    __try {
#endif
        std::uint8_t mode{};
        if (live_binding_list(list, config, mode)) {
            targeted = true;
            failure_mode = mode;
            const auto* const begin =
                field<const std::byte*>(list, kListBeginOffset);
            const auto* const end =
                field<const std::byte*>(list, kListEndOffset);
            const auto* const capacity =
                field<const std::byte*>(list, kListCapacityOffset);
            if (begin && end && capacity && begin <= end &&
                end <= capacity &&
                static_cast<std::size_t>(end - begin) %
                    kKeySettingSize == 0 &&
                static_cast<std::size_t>(capacity - begin) %
                    kKeySettingSize == 0) {
                initial_count = static_cast<std::uint32_t>(
                    (end - begin) / kKeySettingSize);
                initial_capacity = static_cast<std::uint32_t>(
                    (capacity - begin) / kKeySettingSize);
            }
        }
        if (targeted) {
            std::vector<KeySettingStorage>& scratch =
                g_cell_scratch[mode];
            if (scratch.empty()) {
                // This catalog contributes no actions to this native screen.
                accepted = true;
            } else {
                failure_stage = static_cast<std::uint32_t>(
                    BindingBuilderStage::definitions);
                if (initialize_mode_definitions(
                        list, mode, &definition_scan)) {
                    failure_stage = static_cast<std::uint32_t>(
                        BindingBuilderStage::scratch);
                    if (build_scratch_cells(mode)) {
                        // From this point forward a native vector may retain
                        // our pointers, even when append faults partway
                        // through. Teardown must retain all backing records
                        // and mutation guards for process lifetime.
                        g_rows_published.store(
                            true, std::memory_order_release);
                        void* const vector =
                            static_cast<std::byte*>(list) +
                            kListVectorOffset;
                        failure_stage = static_cast<std::uint32_t>(
                            BindingBuilderStage::append);
                        for (std::size_t index = 0;
                             index < scratch.size();
                             ++index) {
                            failure_cell =
                                static_cast<std::uint32_t>(index);
                            g_addresses.append_key_setting(
                                vector, scratch[index].bytes.data());
                        }
                        failure_stage = static_cast<std::uint32_t>(
                            BindingBuilderStage::tail_validation);
                        accepted = validate_appended_tail(list, scratch);

                        const auto* const final_begin =
                            field<const std::byte*>(
                                list, kListBeginOffset);
                        const auto* const final_end =
                            field<const std::byte*>(
                                list, kListEndOffset);
                        const auto* const final_capacity_pointer =
                            field<const std::byte*>(
                                list, kListCapacityOffset);
                        if (final_begin && final_end &&
                            final_capacity_pointer &&
                            final_begin <= final_end &&
                            final_end <= final_capacity_pointer &&
                            static_cast<std::size_t>(
                                final_end - final_begin) %
                                    kKeySettingSize == 0 &&
                            static_cast<std::size_t>(
                                final_capacity_pointer - final_begin) %
                                    kKeySettingSize == 0) {
                            final_count = static_cast<std::uint32_t>(
                                (final_end - final_begin) /
                                    kKeySettingSize);
                            final_capacity = static_cast<std::uint32_t>(
                                (final_capacity_pointer - final_begin) /
                                    kKeySettingSize);
                        }
                    }
                }
            }
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        targeted = true;
        accepted = false;
        seh_code = static_cast<std::uint32_t>(GetExceptionCode());
    }
#endif
    if (targeted && !accepted) {
        g_builder_failure_stage.store(
            failure_stage, std::memory_order_relaxed);
        g_builder_failure_mode.store(
            failure_mode, std::memory_order_relaxed);
        g_builder_failure_cell.store(
            failure_cell, std::memory_order_relaxed);
        g_builder_failure_exception.store(
            seh_code, std::memory_order_relaxed);
        g_builder_failure_initial_count.store(
            initial_count, std::memory_order_relaxed);
        g_builder_failure_initial_capacity.store(
            initial_capacity, std::memory_order_relaxed);
        g_builder_failure_final_count.store(
            final_count, std::memory_order_relaxed);
        g_builder_failure_final_capacity.store(
            final_capacity, std::memory_order_relaxed);
        g_builder_failure_definition_scan = definition_scan;
        g_builder_failed.store(true, std::memory_order_release);
    }
    g_builder_busy.clear(std::memory_order_release);
}

void __fastcall build_key_setting_list_detour(
    void* list,
    void* config) noexcept {
    const BuildKeySettingListFn original =
        g_original_build.load(std::memory_order_acquire);
    if (!original) return;
    original(list, config);
    append_compiled_bindings(list, config);
}

const OwnershipEntry* owned_entry_for_item(
    void* item,
    void* config) noexcept {
    if (!item || !config ||
        field<void*>(config, 0) != g_addresses.key_config_vtable) {
        return nullptr;
    }
    const std::uint8_t mode =
        field<std::uint8_t>(config, kConfigModeOffset);
    if (mode >= kConfigModeCount) return nullptr;
    const auto* const definition =
        field<const KeyBindingDefinition*>(
            item, kItemDefinitionOffset);
    BindingValue* const value =
        field<BindingValue*>(item, kItemBindingOffset);
    const std::uint32_t kind =
        field<std::uint32_t>(item, kItemKindOffset);
    if (!definition || !value || kind > kMouseKind) return nullptr;

    const auto less_definition = [](
        const OwnershipEntry& entry,
        const KeyBindingDefinition* needle) {
        return std::less<const void*>{}(
            static_cast<const void*>(entry.definition),
            static_cast<const void*>(needle));
    };
    const auto found = std::lower_bound(
        g_ownership.begin(),
        g_ownership.end(),
        definition,
        less_definition);
    if (found == g_ownership.end() ||
        found->definition != definition ||
        found->value != value ||
        found->mode != mode ||
        definition->action_id != kUnownedNativeActionId) {
        return nullptr;
    }
    return &*found;
}

bool item_kind_supported(
    const OwnershipEntry& entry,
    std::uint32_t kind) noexcept {
    return kind <= kMouseKind &&
        (entry.allowed_kind_mask &
            static_cast<std::uint8_t>(1u << kind)) != 0;
}

void enqueue_assignment_change(
    NativeBinding& binding,
    std::uint32_t revision,
    const ERUI_ActionInputs& previous,
    const ERUI_ActionInputs& current,
    ERUI_AssignmentChangeReason reason) noexcept {
    const ERUI_InputDevices changed =
        erui::detail::changed_input_devices(previous, current);
    if (changed == ERUI_INPUT_DEVICE_NONE) {
        return;
    }
    if (!g_assignment_events.push({
            &binding, revision, previous, current, reason, changed})) {
        g_dropped_assignment_events.fetch_add(
            1, std::memory_order_relaxed);
    }
}

void* __fastcall poll_key_capture_detour(
    void* item,
    void* result,
    void* config,
    void* cancel_input) noexcept {
    const PollKeyCaptureFn original =
        g_original_capture_poll.load(std::memory_order_acquire);
    if (!original) return result;

    // This global state intentionally covers official rows as well. No
    // ERNativeUI action may fire while the native remapping job owns input.
    g_remap_capture_active.store(true, std::memory_order_release);
    const OwnershipEntry* ownership = nullptr;
    bool owned_thread_safe = true;
#if defined(_MSC_VER)
    __try {
#endif
        ownership = owned_entry_for_item(item, config);
        if (ownership) owned_thread_safe = claim_input_mutation_thread();
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        owned_thread_safe = false;
        flag_runtime_failure();
    }
#endif
    void* const native_result =
        original(item, result, config, cancel_input);
    if (!native_result) {
        g_remap_capture_active.store(false, std::memory_order_release);
        return native_result;
    }

    bool terminal = false;
#if defined(_MSC_VER)
    __try {
#endif
        const std::uint32_t result_kind =
            field<std::uint32_t>(native_result, 0);
        terminal = result_kind > 1;
        if (ownership && result_kind == 2) {
            const std::uint32_t kind =
                field<std::uint32_t>(item, kItemKindOffset);
            field<std::uint16_t>(item, kItemConflictOffset) = 0;
            bool accepted = false;
            if (owned_thread_safe &&
                item_kind_supported(*ownership, kind)) {
                const std::size_t token_offset =
                    kind == kKeyboardKind
                    ? kItemKeyboardTokenOffset
                    : kItemLatestTokenOffset;
                const InputToken token = *reinterpret_cast<const InputToken*>(
                    static_cast<const std::byte*>(item) + token_offset);
                // The semantic decoder rejects unknown tokens and all
                // modifier chords. API 1.1 never accepts a binding that
                // would be visible but unable to activate.
                for (unsigned attempt = 0; attempt < 16 && !accepted;
                     ++attempt) {
                    VersionedActionInputs desired{};
                    if (!ownership->binding->desired_inputs.try_load(
                            desired)) {
                        continue;
                    }
                    ERUI_ActionInputs next = desired.value;
                    BindingValue encoded{};
                    if (!assign_captured_token(next, kind, token) ||
                        !encode_binding_value(next, encoded)) {
                        break;
                    }
                    std::uint32_t revision{};
                    if (!ownership->binding->desired_inputs.
                            publish_if_revision(
                                desired.revision, next, revision)) {
                        continue;
                    }
                    // Capture/clear and input sampling are required to share
                    // one native mutation thread. Only the input-manager
                    // publishes runtime snapshots; this assignment merely
                    // canonicalizes the value retained by the open UI.
                    ownership->binding->value = encoded;
                    accepted = true;
                    enqueue_assignment_change(
                        *ownership->binding,
                        revision,
                        desired.value,
                        next,
                        ERUI_ASSIGNMENT_CHANGE_PLAYER_ASSIGNMENT);
                }
            }
            if (!accepted) {
                // The native poller writes directly through the item's value
                // pointer before returning success. Unsupported cells,
                // modifier chords, and publication contention must therefore
                // restore the latest canonical snapshot; otherwise a rejected
                // edit would remain visible and could leak into native UI
                // behavior despite never becoming an ERNativeUI assignment.
                VersionedActionInputs latest{};
                if (!g_runtime_failed.load(std::memory_order_acquire) &&
                    ownership->binding->desired_inputs.try_load(latest)) {
                    (void)write_action_inputs_value(
                        *ownership->binding,
                        latest.value);
                }
            }
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        terminal = true;
        flag_runtime_failure();
    }
#endif
    if (terminal) {
        // Release only after the complete three-device snapshot is visible.
        g_remap_capture_active.store(false, std::memory_order_release);
    }
    return native_result;
}

void __fastcall clear_key_setting_detour(
    void* item,
    void* config) noexcept {
    const ClearKeySettingFn original =
        g_original_clear.load(std::memory_order_acquire);
    if (!original) return;
#if defined(_MSC_VER)
    __try {
#endif
        const OwnershipEntry* const ownership =
            owned_entry_for_item(item, config);
        if (ownership) {
            const std::uint32_t kind =
                field<std::uint32_t>(item, kItemKindOffset);
            field<std::uint16_t>(item, kItemConflictOffset) = 0;
            if (claim_input_mutation_thread() &&
                item_kind_supported(*ownership, kind)) {
                for (unsigned attempt = 0; attempt < 16; ++attempt) {
                    VersionedActionInputs desired{};
                    if (!ownership->binding->desired_inputs.try_load(
                            desired)) {
                        continue;
                    }
                    ERUI_ActionInputs next = desired.value;
                    BindingValue encoded{};
                    if (!clear_input_slot(next, kind) ||
                        !encode_binding_value(next, encoded)) {
                        break;
                    }
                    std::uint32_t revision{};
                    if (!ownership->binding->desired_inputs.
                            publish_if_revision(
                                desired.revision, next, revision)) {
                        continue;
                    }
                    ownership->binding->value = encoded;
                    enqueue_assignment_change(
                        *ownership->binding,
                        revision,
                        desired.value,
                        next,
                        ERUI_ASSIGNMENT_CHANGE_PLAYER_CLEAR);
                    break;
                }
            }
            return;
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        flag_runtime_failure();
        return;
    }
#endif
    original(item, config);
}

void __fastcall refresh_key_conflict_detour(
    void* item,
    void* config) noexcept {
    const RefreshKeyConflictFn original =
        g_original_conflict.load(std::memory_order_acquire);
    if (!original) return;
#if defined(_MSC_VER)
    __try {
#endif
        if (owned_entry_for_item(item, config)) {
            // Duplicate ERNativeUI bindings are deliberate. They must not be
            // compared with the game's fixed 54-action configuration bank.
            field<std::uint16_t>(item, kItemConflictOffset) = 0;
            return;
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Unknown data remains native-owned.
    }
#endif
    original(item, config);
}

void reset_all_edge_trackers() noexcept {
    for (const auto& binding : g_bindings) {
        binding->edge_tracker.reset();
    }
}

bool apply_staged_inputs() noexcept {
    for (const auto& binding : g_bindings) {
        VersionedActionInputs desired{};
        if (!binding->desired_inputs.try_load(desired)) {
            continue;
        }
        if (binding->applied_revision.load(std::memory_order_acquire) ==
            desired.revision) {
            continue;
        }
        if (!apply_action_inputs(*binding, desired.value, desired.revision)) {
            return false;
        }
        // Rebinding invalidates all pending released/pressed observations.
        binding->edge_tracker.reset();
    }
    return true;
}

void sample_runtime_bindings(void* input_manager) noexcept {
    if (!g_dispatch_enabled.load(std::memory_order_acquire) ||
        g_runtime_failed.load(std::memory_order_acquire) ||
        !input_manager || !g_addresses.input_manager_slot ||
        *g_addresses.input_manager_slot != input_manager) {
        return;
    }
    const bool suppressed =
        g_remap_capture_active.load(std::memory_order_acquire) ||
        native_text_editor_active() ||
        native_dialog_owns_menu_input();
    if (suppressed) {
        reset_all_edge_trackers();
        return;
    }
    if (!claim_input_mutation_thread() || !apply_staged_inputs()) {
        reset_all_edge_trackers();
        return;
    }

    std::size_t record_count = 0;
    for (std::size_t binding_index = 0;
         binding_index < g_bindings.size();
         ++binding_index) {
        g_active_slots[binding_index] = {};
        VersionedRuntimeBindingSnapshot snapshot{};
        if (!g_bindings[binding_index]->
                runtime_publication.try_load(snapshot)) {
            g_bindings[binding_index]->edge_tracker.reset();
            continue;
        }
        for (std::size_t slot = 0;
             slot < kRuntimeBindingSlotCount;
             ++slot) {
            if (!snapshot.value.slots[slot].bound()) continue;
            if (record_count >= g_query_records.size()) {
                flag_runtime_failure();
                return;
            }
            InputQueryRecord& record = g_query_records[record_count];
            record = {};
            record.physical_id =
                snapshot.value.slots[slot].physical_id;
            record.analog = snapshot.value.slots[slot].analog;
            g_query_mapping[record_count] = {
                static_cast<std::uint32_t>(binding_index),
                static_cast<std::uint8_t>(slot)};
            ++record_count;
        }
    }

    if (record_count != 0) {
        void* const player_input =
            g_addresses.get_player_input(input_manager, 0);
        if (!player_input || !field<void*>(player_input, 0x10) ||
            record_count >
                static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max())) {
            reset_all_edge_trackers();
            return;
        }
        g_addresses.query_input_states(
            player_input,
            g_query_records.data(),
            static_cast<std::int32_t>(record_count));
        for (std::size_t index = 0; index < record_count; ++index) {
            const InputQueryRecord& record = g_query_records[index];
            const bool analog_active =
                std::isfinite(record.analog_value) &&
                std::fabs(record.analog_value) >
                    kAnalogActivationEpsilon;
            const QueryMapping mapping = g_query_mapping[index];
            if (mapping.binding_index >= g_active_slots.size() ||
                mapping.slot >= kRuntimeBindingSlotCount) {
                flag_runtime_failure();
                return;
            }
            g_active_slots[mapping.binding_index][mapping.slot] =
                record.digital_active != 0 || analog_active ? 1u : 0u;
        }
    }

    for (std::size_t binding_index = 0;
         binding_index < g_bindings.size();
         ++binding_index) {
        NativeBinding& binding = *g_bindings[binding_index];
        VersionedRuntimeBindingSnapshot snapshot{};
        if (!binding.runtime_publication.try_load(snapshot)) {
            binding.edge_tracker.reset();
            continue;
        }
        std::array<bool, kRuntimeBindingSlotCount> active{};
        for (std::size_t slot = 0; slot < active.size(); ++slot) {
            active[slot] = g_active_slots[binding_index][slot] != 0;
        }
        const RuntimeBindingDeviceMask devices =
            binding.edge_tracker.sample_mask(snapshot, active);
        if (devices == 0) continue;
        const std::uint32_t assignment_revision =
            binding.applied_revision.load(std::memory_order_acquire);
        if (!g_events.push({&binding, assignment_revision, devices})) {
            g_dropped_events.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void __fastcall input_manager_update_detour(
    void* input_manager,
    void* frame_context) noexcept {
    const InputManagerUpdateFn original =
        g_original_input_update.load(std::memory_order_acquire);
    if (!original) return;
    original(input_manager, frame_context);
#if defined(_MSC_VER)
    __try {
#endif
        sample_runtime_bindings(input_manager);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        flag_runtime_failure();
        reset_all_edge_trackers();
    }
#endif
}

void reset_hook_objects_unpublished() noexcept {
    g_build_hook.reset();
    g_input_update_hook.reset();
    g_text_input_dialog_construct_hook.reset();
    g_software_keyboard_construct_hook.reset();
    g_text_input_dialog_destroy_hook.reset();
    g_software_keyboard_destroy_hook.reset();
    g_conflict_hook.reset();
    g_clear_hook.reset();
    g_capture_poll_hook.reset();
    g_original_build.store(nullptr, std::memory_order_release);
    g_original_input_update.store(nullptr, std::memory_order_release);
    g_original_text_input_dialog_construct.store(
        nullptr, std::memory_order_release);
    g_original_software_keyboard_construct.store(
        nullptr, std::memory_order_release);
    g_original_text_input_dialog_destroy.store(
        nullptr, std::memory_order_release);
    g_original_software_keyboard_destroy.store(
        nullptr, std::memory_order_release);
    g_original_conflict.store(nullptr, std::memory_order_release);
    g_original_clear.store(nullptr, std::memory_order_release);
    g_original_capture_poll.store(nullptr, std::memory_order_release);
    g_text_editor_tracking_ready.store(false, std::memory_order_release);
}

void reset_prepared_storage_unpublished() noexcept {
    g_events.reset_unpublished();
    g_assignment_events.reset_unpublished();
    g_active_slots.clear();
    g_query_mapping.clear();
    g_query_records.clear();
    for (auto& scratch : g_cell_scratch) scratch.clear();
    g_ownership.clear();
    g_handles.clear();
    g_sections.clear();
    g_bindings.clear();
    g_addresses = {};
    g_prepared.store(false, std::memory_order_release);
}

bool enable_hook(
    SafetyHookInline& hook,
    const char* description) noexcept {
    if (auto enabled = hook.enable(); enabled) return true;
    erui::detail::logf(
        erui::LogLevel::error,
        "Input binding %s hook could not be enabled",
        description);
    return false;
}

bool create_hooks() noexcept {
    if (g_captured_addresses_approved.load(std::memory_order_acquire) &&
        !validate_captured_native_addresses(
            g_captured_addresses, g_captured_allowed_owner)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Solid Uncapper input-binding entries changed after chain approval");
        return false;
    }

    auto capture = SafetyHookInline::create(
        reinterpret_cast<void*>(g_addresses.poll_key_capture),
        reinterpret_cast<void*>(&poll_key_capture_detour),
        SafetyHookInline::StartDisabled);
    auto clear = SafetyHookInline::create(
        reinterpret_cast<void*>(g_addresses.clear_key_setting),
        reinterpret_cast<void*>(&clear_key_setting_detour),
        SafetyHookInline::StartDisabled);
    auto conflict = SafetyHookInline::create(
        reinterpret_cast<void*>(g_addresses.refresh_key_conflict),
        reinterpret_cast<void*>(&refresh_key_conflict_detour),
        SafetyHookInline::StartDisabled);
    auto input_update = SafetyHookInline::create(
        reinterpret_cast<void*>(g_addresses.input_manager_update),
        reinterpret_cast<void*>(&input_manager_update_detour),
        SafetyHookInline::StartDisabled);
    auto software_keyboard_construct = SafetyHookInline::create(
        reinterpret_cast<void*>(g_addresses.software_keyboard_construct),
        reinterpret_cast<void*>(&software_keyboard_construct_detour),
        SafetyHookInline::StartDisabled);
    auto software_keyboard_destroy = SafetyHookInline::create(
        reinterpret_cast<void*>(g_addresses.software_keyboard_destroy),
        reinterpret_cast<void*>(&software_keyboard_destroy_detour),
        SafetyHookInline::StartDisabled);
    auto text_input_construct = SafetyHookInline::create(
        reinterpret_cast<void*>(g_addresses.text_input_dialog_construct),
        reinterpret_cast<void*>(&text_input_dialog_construct_detour),
        SafetyHookInline::StartDisabled);
    auto text_input_destroy = SafetyHookInline::create(
        reinterpret_cast<void*>(g_addresses.text_input_dialog_destroy),
        reinterpret_cast<void*>(&text_input_dialog_destroy_detour),
        SafetyHookInline::StartDisabled);
    auto build = SafetyHookInline::create(
        reinterpret_cast<void*>(g_addresses.build_key_setting_list),
        reinterpret_cast<void*>(&build_key_setting_list_detour),
        SafetyHookInline::StartDisabled);
    if (!capture || !clear || !conflict || !input_update ||
        !software_keyboard_construct || !software_keyboard_destroy ||
        !text_input_construct || !text_input_destroy || !build) {
        return false;
    }

    g_capture_poll_hook = std::move(*capture);
    g_clear_hook = std::move(*clear);
    g_conflict_hook = std::move(*conflict);
    g_input_update_hook = std::move(*input_update);
    g_software_keyboard_construct_hook =
        std::move(*software_keyboard_construct);
    g_software_keyboard_destroy_hook =
        std::move(*software_keyboard_destroy);
    g_text_input_dialog_construct_hook =
        std::move(*text_input_construct);
    g_text_input_dialog_destroy_hook =
        std::move(*text_input_destroy);
    g_build_hook = std::move(*build);

    g_original_capture_poll.store(
        g_capture_poll_hook.original<PollKeyCaptureFn>(),
        std::memory_order_release);
    g_original_clear.store(
        g_clear_hook.original<ClearKeySettingFn>(),
        std::memory_order_release);
    g_original_conflict.store(
        g_conflict_hook.original<RefreshKeyConflictFn>(),
        std::memory_order_release);
    g_original_input_update.store(
        g_input_update_hook.original<InputManagerUpdateFn>(),
        std::memory_order_release);
    g_original_software_keyboard_construct.store(
        g_software_keyboard_construct_hook.original<
            SoftwareKeyboardJobConstructFn>(),
        std::memory_order_release);
    g_original_software_keyboard_destroy.store(
        g_software_keyboard_destroy_hook.original<
            NativeTextEditorDestroyFn>(),
        std::memory_order_release);
    g_original_text_input_dialog_construct.store(
        g_text_input_dialog_construct_hook.original<
            TextInputDialogConstructFn>(),
        std::memory_order_release);
    g_original_text_input_dialog_destroy.store(
        g_text_input_dialog_destroy_hook.original<
            NativeTextEditorDestroyFn>(),
        std::memory_order_release);
    g_original_build.store(
        g_build_hook.original<BuildKeySettingListFn>(),
        std::memory_order_release);
    return true;
}

bool initialize_install_state() noexcept {
    g_builder_failed.store(false, std::memory_order_release);
    g_builder_failure_reported.store(false, std::memory_order_release);
    g_builder_failure_stage.store(0, std::memory_order_release);
    g_builder_failure_mode.store(0xFFFFFFFFu, std::memory_order_release);
    g_builder_failure_cell.store(0xFFFFFFFFu, std::memory_order_release);
    g_builder_failure_exception.store(0, std::memory_order_release);
    g_builder_failure_initial_count.store(
        0xFFFFFFFFu, std::memory_order_release);
    g_builder_failure_initial_capacity.store(
        0xFFFFFFFFu, std::memory_order_release);
    g_builder_failure_final_count.store(
        0xFFFFFFFFu, std::memory_order_release);
    g_builder_failure_final_capacity.store(
        0xFFFFFFFFu, std::memory_order_release);
    g_builder_failure_definition_scan = {};
    g_runtime_failed.store(false, std::memory_order_release);
    g_remap_capture_active.store(false, std::memory_order_release);
    g_text_editor_tracking_ready.store(false, std::memory_order_release);
    g_text_editor_tracking_failed.store(false, std::memory_order_release);
    g_input_thread.store(0, std::memory_order_release);
    g_dropped_events.store(0, std::memory_order_release);
    g_dropped_assignment_events.store(0, std::memory_order_release);
    g_callback_faults.store(0, std::memory_order_release);
    for (auto& slot : g_active_text_editors) {
        slot.store(nullptr, std::memory_order_release);
    }
    for (auto& initialized : g_mode_initialized) {
        initialized.store(false, std::memory_order_release);
    }
    for (const auto& binding : g_bindings) {
        binding->edge_tracker.reset();
    }
    return true;
}

NativeBinding* find_native_binding(
    ERUI_InputActionHandle action) noexcept {
    const auto found = std::lower_bound(
        g_handles.begin(),
        g_handles.end(),
        action,
        [](const HandleEntry& entry, ERUI_InputActionHandle needle) {
            return entry.handle < needle;
        });
    return found != g_handles.end() && found->handle == action
        ? found->binding
        : nullptr;
}

ERUI_Result validate_live_action(
    ERUI_InputActionHandle action,
    NativeBinding*& binding) noexcept {
    binding = nullptr;
    if (action == ERUI_INVALID_INPUT_ACTION) {
        return ERUI_INVALID_ARGUMENT;
    }
    if (!g_prepared.load(std::memory_order_acquire) ||
        !g_installed.load(std::memory_order_acquire)) {
        return ERUI_HOST_NOT_READY;
    }
    if (g_runtime_failed.load(std::memory_order_acquire)) {
        return ERUI_HOST_FAILED;
    }
    binding = find_native_binding(action);
    return binding ? ERUI_OK : ERUI_INVALID_HANDLE;
}

} // namespace

bool capture_native_input_bindings_before_third_party_hooks() noexcept {
    g_captured_addresses_approved.store(false, std::memory_order_release);
    g_captured_addresses_ready.store(false, std::memory_order_release);
    g_captured_addresses = {};
    g_captured_function_entries = {};
    g_captured_allowed_owner = nullptr;

    NativeInputBindingAddresses captured{};
    if (!resolve_native_addresses(captured, true)) return false;
    const auto entries = native_function_entries(captured);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (!entries[index]) return false;
        g_captured_function_entries[index].address = entries[index];
        std::memcpy(
            g_captured_function_entries[index].bytes.data(),
            entries[index],
            g_captured_function_entries[index].bytes.size());
    }
    g_captured_addresses = captured;
    g_captured_addresses_ready.store(true, std::memory_order_release);
    erui::detail::logf(
        erui::LogLevel::info,
        "Captured native input-binding interfaces before third-party hook installation");
    return true;
}

bool approve_captured_native_input_bindings(
    void* allowed_detour_module) noexcept {
    g_captured_addresses_approved.store(false, std::memory_order_release);
    if (!allowed_detour_module ||
        !g_captured_addresses_ready.load(std::memory_order_acquire)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Solid Uncapper input chaining: no early capture is available");
        return false;
    }
    if (!validate_captured_native_addresses(
            g_captured_addresses,
            static_cast<HMODULE>(allowed_detour_module))) {
        return false;
    }
    g_captured_allowed_owner = static_cast<HMODULE>(allowed_detour_module);
    g_captured_addresses_approved.store(true, std::memory_order_release);
    return true;
}

bool prepare_native_input_bindings(
    const erui::detail::CompiledMenu& menu) noexcept {
    if (g_rows_published.load(std::memory_order_acquire) ||
        g_installed.load(std::memory_order_acquire)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Input binding preparation rejected after native publication");
        return false;
    }
    reset_hook_objects_unpublished();
    reset_prepared_storage_unpublished();

    std::size_t section_count = 0;
    std::size_t binding_count = 0;
    for (const auto& section : menu.input_binding_sections) {
        if (section.bindings.empty()) continue;
        ++section_count;
        binding_count += section.bindings.size();
    }
    if (binding_count == 0) {
        g_prepared.store(true, std::memory_order_release);
        return true;
    }
    if (section_count > kMaximumBindingSections ||
        binding_count > kMaximumBindingActions) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Input binding catalog exceeds the production limit (sections=%zu/%zu bindings=%zu/%zu)",
            section_count,
            kMaximumBindingSections,
            binding_count,
            kMaximumBindingActions);
        return false;
    }
    if (g_captured_addresses_approved.load(std::memory_order_acquire)) {
        if (!validate_captured_native_addresses(
                g_captured_addresses, g_captured_allowed_owner)) {
            g_captured_addresses_approved.store(
                false, std::memory_order_release);
            erui::detail::logf(
                erui::LogLevel::error,
                "Solid Uncapper input-binding entries changed before preparation");
            return false;
        }
        g_addresses = g_captured_addresses;
        erui::detail::logf(
            erui::LogLevel::info,
            "Using verified input-binding interfaces captured before Solid Uncapper hooks");
    } else if (!resolve_native_addresses(g_addresses, true)) {
        return false;
    }

    try {
        g_sections.reserve(section_count);
        g_bindings.reserve(binding_count);
        g_handles.reserve(binding_count);
        std::unordered_map<std::string, bool> identities{};
        identities.reserve(binding_count);

        for (const auto& compiled_section : menu.input_binding_sections) {
            if (compiled_section.bindings.empty()) continue;
            auto section = std::make_unique<NativeBindingSection>();
            section->label_id = compiled_section.label_id;
            for (auto& mode_bindings : section->bindings) {
                mode_bindings.reserve(compiled_section.bindings.size());
            }

            for (const auto& compiled_binding :
                 compiled_section.bindings) {
                if (compiled_binding.provider_id.empty() ||
                    compiled_binding.binding_id.empty() ||
                    compiled_binding.provider_id.size() >
                        kMaximumIdentifierBytes ||
                    compiled_binding.binding_id.size() >
                        kMaximumIdentifierBytes ||
                    compiled_binding.handle == ERUI_INVALID_INPUT_ACTION ||
                    !compiled_binding.action ||
                    !compiled_binding.assignments_changed ||
                    !erui::detail::valid_action_default_inputs(
                        compiled_binding.default_inputs) ||
                    !erui::detail::same_supported_input_devices(
                        compiled_binding.current_inputs,
                        compiled_binding.default_inputs)) {
                    throw std::invalid_argument(
                        "invalid compiled input binding");
                }
                const std::string identity = binding_identity(
                    compiled_binding.provider_id,
                    compiled_binding.binding_id);
                if (!identities.emplace(identity, true).second) {
                    throw std::invalid_argument(
                        "duplicate compiled input binding identity");
                }

                auto binding = std::make_unique<NativeBinding>();
                binding->handle = compiled_binding.handle;
                binding->provider_id = compiled_binding.provider_id;
                binding->binding_id = compiled_binding.binding_id;
                binding->label_id = compiled_binding.label_id;
                binding->action = compiled_binding.action;
                binding->assignments_changed =
                    compiled_binding.assignments_changed;
                binding->default_inputs = compiled_binding.default_inputs;
                binding->supported_devices =
                    erui::detail::supported_input_devices(
                        compiled_binding.default_inputs);
                std::uint32_t initial_revision{};
                if (!binding->desired_inputs.publish(
                        compiled_binding.current_inputs,
                        initial_revision) ||
                    !apply_action_inputs(
                        *binding,
                        compiled_binding.current_inputs,
                        initial_revision)) {
                    throw std::runtime_error(
                        "native semantic binding initialization failed");
                }
                NativeBinding* const raw = binding.get();
                g_bindings.push_back(std::move(binding));
                g_handles.push_back({raw->handle, raw});
                if ((raw->supported_devices &
                        ERUI_INPUT_DEVICE_CONTROLLER) != 0) {
                    section->bindings[kControllerConfigMode].push_back(raw);
                }
                if ((raw->supported_devices &
                        (ERUI_INPUT_DEVICE_KEYBOARD |
                            ERUI_INPUT_DEVICE_MOUSE)) != 0) {
                    section->bindings[kKeyboardMouseConfigMode].push_back(raw);
                }
            }
            g_sections.push_back(std::move(section));
        }

        g_ownership.reserve(g_bindings.size() * kConfigModeCount);
        for (const auto& binding : g_bindings) {
            if ((binding->supported_devices &
                    ERUI_INPUT_DEVICE_CONTROLLER) != 0) {
                g_ownership.push_back({
                    &binding->definitions[kControllerConfigMode],
                    &binding->value,
                    binding.get(),
                    kControllerConfigMode,
                    static_cast<std::uint8_t>(1u << kControllerKind)});
            }
            const ERUI_InputDevices keyboard_mouse =
                binding->supported_devices &
                (ERUI_INPUT_DEVICE_KEYBOARD | ERUI_INPUT_DEVICE_MOUSE);
            if (keyboard_mouse != 0) {
                std::uint8_t allowed{};
                if ((keyboard_mouse & ERUI_INPUT_DEVICE_KEYBOARD) != 0) {
                    allowed |= static_cast<std::uint8_t>(
                        1u << kKeyboardKind);
                }
                if ((keyboard_mouse & ERUI_INPUT_DEVICE_MOUSE) != 0) {
                    allowed |= static_cast<std::uint8_t>(
                        1u << kMouseKind);
                }
                g_ownership.push_back({
                    &binding->definitions[kKeyboardMouseConfigMode],
                    &binding->value,
                    binding.get(),
                    kKeyboardMouseConfigMode,
                    allowed});
            }
        }
        std::sort(
            g_ownership.begin(),
            g_ownership.end(),
            [](const OwnershipEntry& left, const OwnershipEntry& right) {
                return std::less<const void*>{}(
                    static_cast<const void*>(left.definition),
                    static_cast<const void*>(right.definition));
            });

        std::sort(
            g_handles.begin(),
            g_handles.end(),
            [](const HandleEntry& left, const HandleEntry& right) {
                return left.handle < right.handle;
            });
        for (std::size_t index = 1; index < g_handles.size(); ++index) {
            if (g_handles[index - 1].handle == g_handles[index].handle) {
                throw std::invalid_argument(
                    "duplicate compiled input action handle");
            }
        }

        for (std::size_t mode = 0; mode < kConfigModeCount; ++mode) {
            std::size_t cells{};
            for (const auto& section : g_sections) {
                if (!section->bindings[mode].empty()) {
                    cells += (section->bindings[mode].size() + 1) * 2;
                }
            }
            g_cell_scratch[mode].resize(cells);
        }
        g_query_records.resize(
            g_bindings.size() * kRuntimeBindingSlotCount);
        g_query_mapping.resize(g_query_records.size());
        g_active_slots.resize(g_bindings.size());
        const std::size_t event_capacity = std::min<std::size_t>(
            g_bindings.size() * 4 + 1,
            kMaximumBindingActions * 4 + 1);
        const std::size_t queue_capacity =
            std::max<std::size_t>(event_capacity, 65);
        if (!g_events.initialize(queue_capacity) ||
            !g_assignment_events.initialize(queue_capacity)) {
            throw std::bad_alloc();
        }
    } catch (const std::exception& exception) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Input binding preparation failed: %s",
            exception.what());
        reset_prepared_storage_unpublished();
        return false;
    } catch (...) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Input binding preparation failed with an unknown exception");
        reset_prepared_storage_unpublished();
        return false;
    }

    g_prepared.store(true, std::memory_order_release);
    erui::detail::logf(
        erui::LogLevel::info,
        "Prepared native input bindings: sections=%zu bindings=%zu controllerCells=%zu keyboardMouseCells=%zu",
        g_sections.size(),
        g_bindings.size(),
        g_cell_scratch[kControllerConfigMode].size(),
        g_cell_scratch[kKeyboardMouseConfigMode].size());
    return true;
}

bool install_native_input_bindings() noexcept {
    if (!g_prepared.load(std::memory_order_acquire)) {
        erui::detail::logf(
            erui::LogLevel::error,
            "Input binding installation requires successful preparation");
        return false;
    }
    if (g_bindings.empty()) return true;
    if (g_installed.exchange(true, std::memory_order_acq_rel)) {
        return false;
    }
    initialize_install_state();
    if (!create_hooks()) {
        reset_hook_objects_unpublished();
        g_installed.store(false, std::memory_order_release);
        return false;
    }

    // Destruction tracking precedes construction tracking. Only after both
    // editor families are complete may global binding dispatch become live.
    if (!enable_hook(
            g_software_keyboard_destroy_hook,
            "SoftwareKeyboardJob destructor") ||
        !enable_hook(
            g_text_input_dialog_destroy_hook,
            "TextInputDialog destructor") ||
        !enable_hook(
            g_software_keyboard_construct_hook,
            "SoftwareKeyboardJob constructor") ||
        !enable_hook(
            g_text_input_dialog_construct_hook,
            "TextInputDialog constructor")) {
        reset_hook_objects_unpublished();
        g_installed.store(false, std::memory_order_release);
        return false;
    }
    g_text_editor_tracking_ready.store(true, std::memory_order_release);

    // All host-owned data boundaries must be guarded before the builder can
    // make a definition/value pointer visible to Elden Ring.
    if (!enable_hook(g_capture_poll_hook, "capture commit") ||
        !enable_hook(g_clear_hook, "clear") ||
        !enable_hook(g_conflict_hook, "conflict policy") ||
        !enable_hook(g_input_update_hook, "global input update")) {
        reset_hook_objects_unpublished();
        g_installed.store(false, std::memory_order_release);
        return false;
    }
    g_dispatch_enabled.store(true, std::memory_order_release);
    if (!enable_hook(g_build_hook, "list builder")) {
        g_dispatch_enabled.store(false, std::memory_order_release);
        reset_hook_objects_unpublished();
        g_installed.store(false, std::memory_order_release);
        return false;
    }

    erui::detail::logf(
        erui::LogLevel::info,
        "Native input binding backend active (sections=%zu bindings=%zu)",
        g_sections.size(),
        g_bindings.size());
    return true;
}

ERUI_Result stage_native_input_action_inputs(
    ERUI_InputActionHandle action,
    const ERUI_ActionInputs& patch,
    ERUI_ActionInputs& complete_inputs) noexcept {
    if (!erui::detail::valid_action_inputs(patch)) {
        return ERUI_INVALID_ARGUMENT;
    }
    NativeBinding* binding{};
    const ERUI_Result live = validate_live_action(action, binding);
    if (live != ERUI_OK) return live;

    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        VersionedActionInputs current{};
        if (!binding->desired_inputs.try_load(current)) continue;
        ERUI_ActionInputs next{};
        if (!erui::detail::overlay_action_inputs(
                current.value, patch, next)) {
            return ERUI_INVALID_ARGUMENT;
        }
        std::uint32_t revision{};
        if (binding->desired_inputs.publish_if_revision(
                current.revision, next, revision)) {
            complete_inputs = next;
            return ERUI_OK;
        }
    }
    return ERUI_QUEUE_FULL;
}

ERUI_Result reset_native_input_action_inputs(
    ERUI_InputActionHandle action,
    ERUI_InputDevices devices,
    ERUI_ActionInputs& complete_inputs) noexcept {
    if (devices == ERUI_INPUT_DEVICE_NONE ||
        (devices & ~static_cast<ERUI_InputDevices>(
            ERUI_INPUT_DEVICE_ALL)) != 0) {
        return ERUI_INVALID_ARGUMENT;
    }
    NativeBinding* binding{};
    const ERUI_Result live = validate_live_action(action, binding);
    if (live != ERUI_OK) return live;

    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        VersionedActionInputs current{};
        if (!binding->desired_inputs.try_load(current)) continue;
        ERUI_ActionInputs next{};
        if (!erui::detail::reset_action_inputs_to_defaults(
                current.value, binding->default_inputs, devices, next)) {
            return ERUI_INVALID_ARGUMENT;
        }
        std::uint32_t revision{};
        if (binding->desired_inputs.publish_if_revision(
                current.revision, next, revision)) {
            complete_inputs = next;
            return ERUI_OK;
        }
    }
    return ERUI_QUEUE_FULL;
}

ERUI_Result get_native_input_action_inputs(
    ERUI_InputActionHandle action,
    ERUI_ActionInputs& complete_inputs) noexcept {
    NativeBinding* binding{};
    const ERUI_Result live = validate_live_action(action, binding);
    if (live != ERUI_OK) return live;
    VersionedActionInputs current{};
    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        if (binding->desired_inputs.try_load(current)) {
            complete_inputs = current.value;
            return ERUI_OK;
        }
    }
    return ERUI_QUEUE_FULL;
}

void dispatch_native_input_binding_events() noexcept {
    if (g_builder_failed.load(std::memory_order_acquire) &&
        !g_builder_failure_reported.exchange(
            true, std::memory_order_acq_rel)) {
        const std::uint32_t failure_stage =
            g_builder_failure_stage.load(std::memory_order_relaxed);
        erui::detail::logf(
            erui::LogLevel::error,
            "Input binding row injection failed closed: stage=%u mode=%u cell=%u exception=0x%08X nativeBefore={count:%u,capacity:%u} nativeAfter={count:%u,capacity:%u} requestedCells=%zu; no further custom binding rows will be appended during this process",
            static_cast<unsigned>(failure_stage),
            static_cast<unsigned>(g_builder_failure_mode.load(
                std::memory_order_relaxed)),
            static_cast<unsigned>(g_builder_failure_cell.load(
                std::memory_order_relaxed)),
            static_cast<unsigned>(g_builder_failure_exception.load(
                std::memory_order_relaxed)),
            static_cast<unsigned>(g_builder_failure_initial_count.load(
                std::memory_order_relaxed)),
            static_cast<unsigned>(g_builder_failure_initial_capacity.load(
                std::memory_order_relaxed)),
            static_cast<unsigned>(g_builder_failure_final_count.load(
                std::memory_order_relaxed)),
            static_cast<unsigned>(g_builder_failure_final_capacity.load(
                std::memory_order_relaxed)),
            g_builder_failure_mode.load(std::memory_order_relaxed) <
                    kConfigModeCount
                ? g_cell_scratch[g_builder_failure_mode.load(
                      std::memory_order_relaxed)].size()
                : 0);
        if (failure_stage == static_cast<std::uint32_t>(
                BindingBuilderStage::definitions)) {
            const DefinitionScanTelemetry scan =
                g_builder_failure_definition_scan;
            const std::uint32_t first_primary_kind =
                scan.first_pair_kinds & 0xFFFFu;
            const std::uint32_t first_secondary_kind =
                (scan.first_pair_kinds >> 16u) & 0xFFFFu;
            erui::detail::logf(
                erui::LogLevel::error,
                "Input binding definition scan: pairs=%u kindPairs=%u category={sentinels:%u,primaryNull:%u,spacers:%u} action={idRange:%u,primaryNonNull:%u,secondaryShape:%u,flags:%u} firstPairKinds={primary:%u,secondary:%u} firstCategory={shape:0x%02X,flags:0x%016llX%016llX} firstAction={id:%u,shape:0x%02X,flags:0x%016llX%016llX}",
                static_cast<unsigned>(scan.pairs_total),
                static_cast<unsigned>(scan.kind_pairs),
                static_cast<unsigned>(scan.category_sentinels),
                static_cast<unsigned>(scan.category_primary_null),
                static_cast<unsigned>(scan.category_spacers),
                static_cast<unsigned>(scan.action_ids_in_range),
                static_cast<unsigned>(scan.action_primary_nonnull),
                static_cast<unsigned>(scan.action_secondary_shape),
                static_cast<unsigned>(scan.action_flags),
                static_cast<unsigned>(first_primary_kind),
                static_cast<unsigned>(first_secondary_kind),
                static_cast<unsigned>(scan.first_category_shape),
                static_cast<unsigned long long>(
                    scan.first_category_flags_high),
                static_cast<unsigned long long>(
                    scan.first_category_flags_low),
                static_cast<unsigned>(scan.first_action_id),
                static_cast<unsigned>(scan.first_action_shape),
                static_cast<unsigned long long>(
                    scan.first_action_flags_high),
                static_cast<unsigned long long>(
                    scan.first_action_flags_low));
        }
    }

    const std::uint32_t dropped =
        g_dropped_events.exchange(0, std::memory_order_acq_rel);
    if (dropped != 0) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Input binding event queue dropped %u edge(s)",
            static_cast<unsigned>(dropped));
    }
    const std::uint32_t dropped_assignments =
        g_dropped_assignment_events.exchange(
            0, std::memory_order_acq_rel);
    if (dropped_assignments != 0) {
        erui::detail::logf(
            erui::LogLevel::warning,
            "Input binding assignment queue dropped %u change(s)",
            static_cast<unsigned>(dropped_assignments));
    }
    if (g_runtime_failed.load(std::memory_order_acquire)) {
        static std::atomic_bool logged{};
        if (!logged.exchange(true, std::memory_order_relaxed)) {
            erui::detail::logf(
                erui::LogLevel::error,
                "Input binding runtime failed closed; native rows remain guarded but callbacks are disabled");
        }
    }
    AssignmentEvent assignment{};
    while (g_assignment_events.pop(assignment)) {
        if (!g_dispatch_enabled.load(std::memory_order_acquire) ||
            !assignment.binding ||
            !assignment.binding->assignments_changed) {
            continue;
        }
        bool invoked = true;
#if defined(_MSC_VER)
        __try {
#endif
            assignment.binding->assignments_changed.invoke(
                assignment.previous,
                assignment.current,
                assignment.reason,
                assignment.changed_devices);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            invoked = false;
        }
#endif
        if (!invoked) {
            const std::uint32_t faults =
                g_callback_faults.fetch_add(
                    1, std::memory_order_relaxed) + 1;
            if (faults <= 8) {
                erui::detail::logf(
                    erui::LogLevel::error,
                    "Input assignment callback faulted safely (%s/%s)",
                    assignment.binding->provider_id.c_str(),
                    assignment.binding->binding_id.c_str());
            }
        }
    }

    BindingEvent event{};
    while (g_events.pop(event)) {
        if (!g_dispatch_enabled.load(std::memory_order_acquire) ||
            g_runtime_failed.load(std::memory_order_acquire) ||
            !event.binding || event.devices == 0 ||
            g_remap_capture_active.load(std::memory_order_acquire) ||
            native_text_editor_active() ||
            native_dialog_owns_menu_input()) {
            continue;
        }
        // A programmatic or player remap invalidates an edge immediately,
        // even during the short interval before the input thread converts
        // and publishes the new runtime snapshot.
        if (event.binding->desired_inputs.revision() != event.revision ||
            event.binding->applied_revision.load(
                std::memory_order_acquire) != event.revision ||
            !event.binding->action) {
            continue;
        }

        bool invoked = true;
#if defined(_MSC_VER)
        __try {
#endif
            event.binding->action.invoke(event.devices);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            invoked = false;
        }
#endif
        if (!invoked) {
            const std::uint32_t faults =
                g_callback_faults.fetch_add(
                    1, std::memory_order_relaxed) + 1;
            if (faults <= 8) {
                erui::detail::logf(
                    erui::LogLevel::error,
                    "Input binding callback faulted safely (%s/%s)",
                    event.binding->provider_id.c_str(),
                    event.binding->binding_id.c_str());
            }
        }
    }
}

void remove_native_input_bindings() noexcept {
    g_dispatch_enabled.store(false, std::memory_order_release);
    g_installed.store(false, std::memory_order_release);
    if (g_rows_published.load(std::memory_order_acquire)) {
        // Prevent future publication, but keep every mutation/editor hook and
        // all backing objects alive. A currently open KeyConfigDialog can
        // retain our raw pointers after this call. The pinned production host
        // intentionally does not support hot unload.
        g_build_hook.reset();
        g_original_build.store(nullptr, std::memory_order_release);
        erui::detail::logf(
            erui::LogLevel::warning,
            "Input binding backend retained native guards and backing storage after removal because rows were already published");
        return;
    }
    reset_hook_objects_unpublished();
    reset_prepared_storage_unpublished();
}

} // namespace erui::native
