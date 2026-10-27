#include "native_tab_runtime_state.hpp"

#include <new>

namespace erui::detail {

bool validate_native_tab_live_geometry(
    std::uint64_t record_count,
    std::int32_t active_slot_count,
    std::int32_t selected_slot,
    std::int32_t horizontal_extent,
    std::int32_t vertical_extent,
    NativeTabLiveGeometry& geometry) noexcept {
    geometry = {};
    constexpr std::int32_t stock_slot_count = 8;
    constexpr std::int32_t maximum_horizontal_extent = 32;
    constexpr std::int32_t maximum_vertical_extent = 64;
    if (active_slot_count < stock_slot_count ||
        active_slot_count >
            static_cast<std::int32_t>(native_tab_physical_slot_capacity) ||
        selected_slot < 0 || selected_slot >= active_slot_count ||
        horizontal_extent <= 0 ||
        horizontal_extent > maximum_horizontal_extent ||
        vertical_extent <= 0 ||
        vertical_extent > maximum_vertical_extent ||
        (horizontal_extent != 1 && vertical_extent != 1) ||
        record_count < static_cast<std::uint64_t>(active_slot_count) ||
        record_count > native_tab_physical_slot_capacity) {
        return false;
    }

    const std::size_t authored_cell_count =
        static_cast<std::size_t>(horizontal_extent) *
        static_cast<std::size_t>(vertical_extent);
    const std::size_t active = static_cast<std::size_t>(active_slot_count);
    if (authored_cell_count <= active) return false;

    geometry.active_slot_count = active;
    geometry.authored_cell_count = authored_cell_count;
    geometry.selected_slot = static_cast<std::size_t>(selected_slot);
    return true;
}

NativeTabPhysicalHostState* NativeTabPhysicalSlotState::host(
    NativeTabPhysicalLayer layer) noexcept {
    switch (layer) {
    case NativeTabPhysicalLayer::front:
        return &front;
    case NativeTabPhysicalLayer::back:
        return &back;
    default:
        return nullptr;
    }
}

const NativeTabPhysicalHostState* NativeTabPhysicalSlotState::host(
    NativeTabPhysicalLayer layer) const noexcept {
    switch (layer) {
    case NativeTabPhysicalLayer::front:
        return &front;
    case NativeTabPhysicalLayer::back:
        return &back;
    default:
        return nullptr;
    }
}

std::unique_ptr<NativeTabRuntimeState> NativeTabRuntimeState::create(
    std::size_t tab_count) noexcept {
    try {
        auto runtime = std::unique_ptr<NativeTabRuntimeState>(
            new NativeTabRuntimeState());
        runtime->tabs_.reserve(tab_count);
        // Retirement updates run inside native UI callbacks and must not
        // allocate there. Reserve only the actual registered tab count while
        // the runtime is still private; allocation failure then fails hook
        // installation closed at create().
        runtime->retiring_tab_indices_.reserve(tab_count);
        for (std::size_t index = 0; index < tab_count; ++index) {
            runtime->tabs_.push_back(
                std::unique_ptr<NativeTabRuntimeEntry>(
                    new NativeTabRuntimeEntry(index)));
        }
        return runtime;
    } catch (...) {
        return nullptr;
    }
}

NativeTabRuntimeEntry* NativeTabRuntimeState::tab(
    std::size_t index) noexcept {
    return index < tabs_.size() ? tabs_[index].get() : nullptr;
}

const NativeTabRuntimeEntry* NativeTabRuntimeState::tab(
    std::size_t index) const noexcept {
    return index < tabs_.size() ? tabs_[index].get() : nullptr;
}

bool NativeTabRuntimeState::begin_physical_epoch(
    std::size_t active_slot_count) noexcept {
    if (active_slot_count == 0 ||
        active_slot_count > native_tab_physical_slot_capacity) {
        return false;
    }

    std::size_t expected = 0;
    return active_physical_slot_count_.compare_exchange_strong(
        expected,
        active_slot_count,
        std::memory_order_release,
        std::memory_order_relaxed);
}

std::span<const NativeTabPhysicalSlotState>
NativeTabRuntimeState::active_physical_slots() const noexcept {
    return std::span<const NativeTabPhysicalSlotState>(
        physical_slots_.data(), active_physical_slot_count());
}

const NativeTabPhysicalSlotState* NativeTabRuntimeState::physical_slot(
    std::size_t physical_slot_index) const noexcept {
    return physical_slot_index < active_physical_slot_count()
        ? &physical_slots_[physical_slot_index]
        : nullptr;
}

NativeTabPhysicalSlotState* NativeTabRuntimeState::mutable_physical_slot(
    std::size_t physical_slot_index) noexcept {
    return physical_slot_index < active_physical_slot_count()
        ? &physical_slots_[physical_slot_index]
        : nullptr;
}

const NativeTabPhysicalHostState* NativeTabRuntimeState::host(
    std::size_t physical_slot_index,
    NativeTabPhysicalLayer layer) const noexcept {
    const NativeTabPhysicalSlotState* const slot =
        physical_slot(physical_slot_index);
    return slot ? slot->host(layer) : nullptr;
}

bool NativeTabRuntimeState::set_active_host(
    std::size_t physical_slot_index,
    NativeTabPhysicalLayer layer,
    std::size_t tab_index) noexcept {
    NativeTabPhysicalSlotState* const slot =
        mutable_physical_slot(physical_slot_index);
    NativeTabPhysicalHostState* const state =
        slot ? slot->host(layer) : nullptr;
    if (!state || tab_index >= tabs_.size()) return false;
    state->active_tab_index.store(tab_index, std::memory_order_release);
    return true;
}

bool NativeTabRuntimeState::set_staged_host(
    std::size_t physical_slot_index,
    NativeTabPhysicalLayer layer,
    std::size_t tab_index) noexcept {
    NativeTabPhysicalSlotState* const slot =
        mutable_physical_slot(physical_slot_index);
    NativeTabPhysicalHostState* const state =
        slot ? slot->host(layer) : nullptr;
    if (!state || tab_index >= tabs_.size()) return false;
    state->staged_tab_index.store(tab_index, std::memory_order_release);
    return true;
}

bool NativeTabRuntimeState::clear_active_host(
    std::size_t physical_slot_index,
    NativeTabPhysicalLayer layer) noexcept {
    NativeTabPhysicalSlotState* const slot =
        mutable_physical_slot(physical_slot_index);
    NativeTabPhysicalHostState* const state =
        slot ? slot->host(layer) : nullptr;
    if (!state) return false;
    state->active_tab_index.store(
        invalid_native_tab_runtime_index, std::memory_order_release);
    return true;
}

bool NativeTabRuntimeState::clear_staged_host(
    std::size_t physical_slot_index,
    NativeTabPhysicalLayer layer) noexcept {
    NativeTabPhysicalSlotState* const slot =
        mutable_physical_slot(physical_slot_index);
    NativeTabPhysicalHostState* const state =
        slot ? slot->host(layer) : nullptr;
    if (!state) return false;
    state->staged_tab_index.store(
        invalid_native_tab_runtime_index, std::memory_order_release);
    return true;
}

bool NativeTabRuntimeState::decode_flattened_host(
    std::size_t host_index,
    std::size_t& physical_slot_index,
    NativeTabPhysicalLayer& layer) const noexcept {
    const std::size_t active_slots = active_physical_slot_count();
    if (active_slots == 0 || host_index >= active_slots * 2U) return false;
    if (host_index < active_slots) {
        physical_slot_index = host_index;
        layer = NativeTabPhysicalLayer::front;
    } else {
        physical_slot_index = host_index - active_slots;
        layer = NativeTabPhysicalLayer::back;
    }
    return true;
}

const NativeTabPhysicalHostState* NativeTabRuntimeState::host(
    std::size_t host_index) const noexcept {
    std::size_t physical_slot_index{};
    NativeTabPhysicalLayer layer{};
    return decode_flattened_host(host_index, physical_slot_index, layer)
        ? host(physical_slot_index, layer)
        : nullptr;
}

bool NativeTabRuntimeState::set_active_host(
    std::size_t host_index,
    std::size_t tab_index) noexcept {
    std::size_t physical_slot_index{};
    NativeTabPhysicalLayer layer{};
    return decode_flattened_host(host_index, physical_slot_index, layer) &&
        set_active_host(physical_slot_index, layer, tab_index);
}

bool NativeTabRuntimeState::set_staged_host(
    std::size_t host_index,
    std::size_t tab_index) noexcept {
    std::size_t physical_slot_index{};
    NativeTabPhysicalLayer layer{};
    return decode_flattened_host(host_index, physical_slot_index, layer) &&
        set_staged_host(physical_slot_index, layer, tab_index);
}

bool NativeTabRuntimeState::clear_active_host(
    std::size_t host_index) noexcept {
    std::size_t physical_slot_index{};
    NativeTabPhysicalLayer layer{};
    return decode_flattened_host(host_index, physical_slot_index, layer) &&
        clear_active_host(physical_slot_index, layer);
}

bool NativeTabRuntimeState::clear_staged_host(
    std::size_t host_index) noexcept {
    std::size_t physical_slot_index{};
    NativeTabPhysicalLayer layer{};
    return decode_flattened_host(host_index, physical_slot_index, layer) &&
        clear_staged_host(physical_slot_index, layer);
}

std::size_t NativeTabRuntimeState::active_host_count(
    std::size_t tab_index) const noexcept {
    if (tab_index >= tabs_.size()) return 0;
    std::size_t result{};
    for (const NativeTabPhysicalSlotState& slot : active_physical_slots()) {
        for (const NativeTabPhysicalLayer layer : {
                 NativeTabPhysicalLayer::front,
                 NativeTabPhysicalLayer::back}) {
            const NativeTabPhysicalHostState* const state = slot.host(layer);
            if (state && state->active_tab_index.load(
                    std::memory_order_acquire) == tab_index) {
                ++result;
            }
        }
    }
    return result;
}

std::size_t NativeTabRuntimeState::staged_host_count(
    std::size_t tab_index) const noexcept {
    if (tab_index >= tabs_.size()) return 0;
    std::size_t result{};
    for (const NativeTabPhysicalSlotState& slot : active_physical_slots()) {
        for (const NativeTabPhysicalLayer layer : {
                 NativeTabPhysicalLayer::front,
                 NativeTabPhysicalLayer::back}) {
            const NativeTabPhysicalHostState* const state = slot.host(layer);
            if (state && state->staged_tab_index.load(
                    std::memory_order_acquire) == tab_index) {
                ++result;
            }
        }
    }
    return result;
}

std::size_t NativeTabRuntimeState::total_host_count(
    std::size_t tab_index) const noexcept {
    return active_host_count(tab_index) + staged_host_count(tab_index);
}

bool NativeTabRuntimeState::set_retirement_debt(
    std::size_t tab_index,
    std::uint32_t debt) noexcept {
    NativeTabRuntimeEntry* const entry = tab(tab_index);
    if (!entry) return false;

    const bool queued = entry->retirement_queue_position_ !=
        invalid_native_tab_runtime_index;
    if (debt != 0 && !queued) {
        try {
            retiring_tab_indices_.push_back(tab_index);
        } catch (...) {
            return false;
        }
        entry->retirement_queue_position_ =
            retiring_tab_indices_.size() - 1;
    } else if (debt == 0 && queued) {
        const std::size_t position = entry->retirement_queue_position_;
        const std::size_t moved_index = retiring_tab_indices_.back();
        if (position + 1 != retiring_tab_indices_.size()) {
            retiring_tab_indices_[position] = moved_index;
            NativeTabRuntimeEntry* const moved_entry = tab(moved_index);
            if (moved_entry) {
                moved_entry->retirement_queue_position_ = position;
            }
        }
        retiring_tab_indices_.pop_back();
        entry->retirement_queue_position_ =
            invalid_native_tab_runtime_index;
    }

    entry->retirement_debt_.store(debt, std::memory_order_release);
    return true;
}

void NativeTabRuntimeState::reset_epoch_tracking() noexcept {
    // Make every public host accessor fail closed before erasing the former
    // epoch's bookkeeping. The caller has already proved that no native owner
    // can retain these presentation hosts.
    active_physical_slot_count_.store(0, std::memory_order_release);
    for (NativeTabPhysicalSlotState& slot : physical_slots_) {
        for (NativeTabPhysicalHostState* const host : {
                 &slot.front, &slot.back}) {
            host->active_tab_index.store(
                invalid_native_tab_runtime_index,
                std::memory_order_release);
            host->staged_tab_index.store(
                invalid_native_tab_runtime_index,
                std::memory_order_release);
        }
    }
    for (const std::size_t index : retiring_tab_indices_) {
        if (NativeTabRuntimeEntry* const entry = tab(index)) {
            entry->retirement_debt_.store(0, std::memory_order_release);
            entry->retirement_queue_position_ =
                invalid_native_tab_runtime_index;
        }
    }
    retiring_tab_indices_.clear();
}

} // namespace erui::detail
