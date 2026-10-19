#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

namespace erui::detail {

// The native Configuration object owns storage for at most ten category
// records. How many of those records are active is a property of the current
// movie epoch and must be supplied from the validated live selector state; it
// is deliberately not encoded as an eight-slot compile-time constant here.
inline constexpr std::size_t native_tab_physical_slot_capacity = 10;
inline constexpr std::size_t native_tab_physical_layer_count = 2;
inline constexpr std::size_t native_tab_physical_host_capacity =
    native_tab_physical_slot_capacity * native_tab_physical_layer_count;
inline constexpr std::size_t invalid_native_tab_runtime_index =
    std::numeric_limits<std::size_t>::max();

// Validated snapshot of the selector fields that define one physical tab
// strip. The native vector count and selector active count must agree; the
// authored GFX strip must be one-dimensional and retain its trailing cell.
struct NativeTabLiveGeometry final {
    std::size_t active_slot_count{};
    std::size_t authored_cell_count{};
    std::size_t selected_slot{};
};

[[nodiscard]] bool validate_native_tab_live_geometry(
    std::uint64_t record_count,
    std::int32_t active_slot_count,
    std::int32_t selected_slot,
    std::int32_t horizontal_extent,
    std::int32_t vertical_extent,
    NativeTabLiveGeometry& geometry) noexcept;

enum class NativeTabPhysicalLayer : std::uint8_t {
    front = 0,
    back = 1,
};

struct NativeTabIconRuntimeState final {
    // Set exactly once while the runtime catalogue is still private, before
    // any native hook can observe it.
    bool raster_requested{};
    std::int32_t resource_id{};
    std::array<wchar_t, 32> resource_name{};
    std::array<wchar_t, 48> raw_resource_name{};

    // The Scaleform lookup/bind detours can observe publication while the UI
    // callback is creating the immutable native resource. Keep the proven
    // release/acquire boundary of the former parallel atomic arrays.
    std::atomic<void*> image{};
    std::atomic<void*> resource{};
    std::atomic_uint64_t checksum{};
    std::atomic_bool ready{};
};

// One stable object owns all mutable native state associated with one logical
// custom tab. The enclosing runtime stores these objects indirectly so their
// addresses never change after publication.
class NativeTabRuntimeEntry final {
public:
    NativeTabRuntimeEntry(const NativeTabRuntimeEntry&) = delete;
    NativeTabRuntimeEntry& operator=(const NativeTabRuntimeEntry&) = delete;
    NativeTabRuntimeEntry(NativeTabRuntimeEntry&&) = delete;
    NativeTabRuntimeEntry& operator=(NativeTabRuntimeEntry&&) = delete;

    [[nodiscard]] std::size_t index() const noexcept { return index_; }

    [[nodiscard]] NativeTabIconRuntimeState& icon() noexcept { return icon_; }
    [[nodiscard]] const NativeTabIconRuntimeState& icon() const noexcept {
        return icon_;
    }

    [[nodiscard]] std::uint32_t retirement_debt() const noexcept {
        return retirement_debt_.load(std::memory_order_acquire);
    }

private:
    explicit NativeTabRuntimeEntry(std::size_t index) noexcept
        : index_(index) {}

    std::size_t index_{};
    NativeTabIconRuntimeState icon_{};
    std::atomic_uint32_t retirement_debt_{};

    // Owned by the native UI/render thread together with
    // retiring_tab_indices_. Keeping the reverse position on the tab itself
    // makes insertion duplicate-safe and removal O(1), without another
    // max-sized side table.
    std::size_t retirement_queue_position_{
        invalid_native_tab_runtime_index};

    friend class NativeTabRuntimeState;
};

struct NativeTabPhysicalHostState final {
    std::atomic_size_t active_tab_index{invalid_native_tab_runtime_index};
    std::atomic_size_t staged_tab_index{invalid_native_tab_runtime_index};

    [[nodiscard]] bool has_active_tab() const noexcept {
        return active_tab_index.load(std::memory_order_acquire) !=
            invalid_native_tab_runtime_index;
    }

    [[nodiscard]] bool has_staged_tab() const noexcept {
        return staged_tab_index.load(std::memory_order_acquire) !=
            invalid_native_tab_runtime_index;
    }
};

// A physical tab is one selectable native category record. Scaleform exposes
// two independent presentation objects for that record: TabList (front) and
// BackTabList (back/transition). Keeping both hosts inside their owning slot
// prevents the former flattened `slot_count * 2` indexing scheme from making
// the two layers look like additional tabs.
struct NativeTabPhysicalSlotState final {
    NativeTabPhysicalHostState front{};
    NativeTabPhysicalHostState back{};

    [[nodiscard]] NativeTabPhysicalHostState* host(
        NativeTabPhysicalLayer layer) noexcept;
    [[nodiscard]] const NativeTabPhysicalHostState* host(
        NativeTabPhysicalLayer layer) const noexcept;
};

// Pure bookkeeping for native custom-tab resources. Construction allocates
// exactly one entry for each tab that was actually registered; the public API
// ceiling does not determine this object's storage or traversal cost.
class NativeTabRuntimeState final {
public:
    NativeTabRuntimeState(const NativeTabRuntimeState&) = delete;
    NativeTabRuntimeState& operator=(const NativeTabRuntimeState&) = delete;
    NativeTabRuntimeState(NativeTabRuntimeState&&) = delete;
    NativeTabRuntimeState& operator=(NativeTabRuntimeState&&) = delete;
    ~NativeTabRuntimeState() = default;

    // This boundary is noexcept so allocation failure can make native hook
    // installation fail closed without unwinding through loader/runtime code.
    [[nodiscard]] static std::unique_ptr<NativeTabRuntimeState> create(
        std::size_t tab_count) noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return tabs_.size(); }
    [[nodiscard]] bool empty() const noexcept { return tabs_.empty(); }

    [[nodiscard]] NativeTabRuntimeEntry* tab(std::size_t index) noexcept;
    [[nodiscard]] const NativeTabRuntimeEntry* tab(
        std::size_t index) const noexcept;

    // Bind the live physical span for one validated System-menu movie epoch.
    // The count is normally selector + 0xD0 after the caller has proved it
    // agrees with the native record count and authored presentation capacity.
    // A second begin is rejected until reset_epoch_tracking() ends the current
    // epoch, avoiding an in-place reinterpretation of live host ownership.
    [[nodiscard]] bool begin_physical_epoch(
        std::size_t active_slot_count) noexcept;
    [[nodiscard]] std::size_t active_physical_slot_count() const noexcept {
        return active_physical_slot_count_.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::span<const NativeTabPhysicalSlotState>
        active_physical_slots() const noexcept;
    [[nodiscard]] const NativeTabPhysicalSlotState* physical_slot(
        std::size_t physical_slot) const noexcept;

    [[nodiscard]] const NativeTabPhysicalHostState* host(
        std::size_t physical_slot,
        NativeTabPhysicalLayer layer) const noexcept;

    [[nodiscard]] bool set_active_host(
        std::size_t physical_slot,
        NativeTabPhysicalLayer layer,
        std::size_t tab_index) noexcept;
    [[nodiscard]] bool set_staged_host(
        std::size_t physical_slot,
        NativeTabPhysicalLayer layer,
        std::size_t tab_index) noexcept;
    [[nodiscard]] bool clear_active_host(
        std::size_t physical_slot,
        NativeTabPhysicalLayer layer) noexcept;
    [[nodiscard]] bool clear_staged_host(
        std::size_t physical_slot,
        NativeTabPhysicalLayer layer) noexcept;

    // Transitional adapters for callers that still enumerate the two layers
    // as [front slots][back slots]. Their extent is derived from the current
    // epoch rather than a fixed sixteen-host constant. New code should use the
    // layer-safe overloads above.
    [[nodiscard]] std::size_t flattened_physical_host_count() const noexcept {
        return active_physical_slot_count() * native_tab_physical_layer_count;
    }
    [[nodiscard]] const NativeTabPhysicalHostState* host(
        std::size_t host_index) const noexcept;

    [[nodiscard]] bool set_active_host(
        std::size_t host_index,
        std::size_t tab_index) noexcept;
    [[nodiscard]] bool set_staged_host(
        std::size_t host_index,
        std::size_t tab_index) noexcept;
    [[nodiscard]] bool clear_active_host(std::size_t host_index) noexcept;
    [[nodiscard]] bool clear_staged_host(std::size_t host_index) noexcept;

    [[nodiscard]] std::size_t active_host_count(
        std::size_t tab_index) const noexcept;
    [[nodiscard]] std::size_t staged_host_count(
        std::size_t tab_index) const noexcept;
    [[nodiscard]] std::size_t total_host_count(
        std::size_t tab_index) const noexcept;

    // A non-zero debt inserts the tab into a sparse worklist exactly once.
    // Updating an already-retiring tab never creates a duplicate. Setting the
    // debt to zero removes only that tab from the worklist. These worklist
    // mutations, retiring_tab_indices(), and reset_epoch_tracking() are owned
    // by the native UI/render thread; retirement_debt() remains safe to
    // observe from another thread through its release/acquire boundary.
    [[nodiscard]] bool set_retirement_debt(
        std::size_t tab_index,
        std::uint32_t debt) noexcept;
    [[nodiscard]] std::span<const std::size_t> retiring_tab_indices()
        const noexcept {
        return retiring_tab_indices_;
    }

    // Call only after native lifetime checks prove that the prior movie epoch
    // no longer owns any hosts or deferred references. Process-owned retained
    // image/resource roots remain intact for reuse in the next epoch.
    void reset_epoch_tracking() noexcept;

private:
    NativeTabRuntimeState() = default;

    [[nodiscard]] NativeTabPhysicalSlotState* mutable_physical_slot(
        std::size_t physical_slot) noexcept;
    [[nodiscard]] bool decode_flattened_host(
        std::size_t host_index,
        std::size_t& physical_slot,
        NativeTabPhysicalLayer& layer) const noexcept;

    std::vector<std::unique_ptr<NativeTabRuntimeEntry>> tabs_{};
    std::array<
        NativeTabPhysicalSlotState,
        native_tab_physical_slot_capacity> physical_slots_{};
    std::atomic_size_t active_physical_slot_count_{};
    std::vector<std::size_t> retiring_tab_indices_{};
};

} // namespace erui::detail
