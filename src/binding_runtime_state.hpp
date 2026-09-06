#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace erui::native {

// One logical action is represented by controller, keyboard, and mouse cells
// that share a BindingValue across the two native configuration screens. Keep
// all alternatives in one immutable input-thread snapshot so changing any
// cell never makes the others disappear at runtime.
constexpr std::size_t kRuntimeBindingSlotCount = 3;

using RuntimeBindingDeviceMask = std::uint32_t;

[[nodiscard]] constexpr RuntimeBindingDeviceMask
runtime_binding_device_bit(std::size_t index) noexcept {
    return index < kRuntimeBindingSlotCount
        ? RuntimeBindingDeviceMask{1} << index
        : RuntimeBindingDeviceMask{0};
}

enum class RuntimeBindingSlotIndex : std::size_t {
    controller = 0,
    keyboard = 1,
    mouse = 2,
};

// These values intentionally equal Elden Ring's native InputToken kind. The
// production backend uses the array index directly when converting a complete
// BindingValue, so an enum reorder must be a compile-time failure.
static_assert(static_cast<std::size_t>(
                  RuntimeBindingSlotIndex::controller) == 0);
static_assert(static_cast<std::size_t>(
                  RuntimeBindingSlotIndex::keyboard) == 1);
static_assert(static_cast<std::size_t>(
                  RuntimeBindingSlotIndex::mouse) == 2);

struct RuntimeBindingSlot {
    std::int32_t physical_id{-1};
    std::uint8_t analog{};

    [[nodiscard]] constexpr bool bound() const noexcept {
        return physical_id != -1;
    }
};

[[nodiscard]] constexpr bool operator==(
    RuntimeBindingSlot left,
    RuntimeBindingSlot right) noexcept {
    return left.physical_id == right.physical_id &&
        left.analog == right.analog;
}

struct RuntimeBindingSnapshot {
    std::array<RuntimeBindingSlot, kRuntimeBindingSlotCount> slots{};
};

[[nodiscard]] constexpr bool operator==(
    const RuntimeBindingSnapshot& left,
    const RuntimeBindingSnapshot& right) noexcept {
    return left.slots == right.slots;
}

struct VersionedRuntimeBindingSnapshot {
    RuntimeBindingSnapshot value{};
    std::uint32_t revision{};
};

// Single-writer, lock-free publication owned by the process-wide native
// input-update thread. Capture/clear only stage semantic assignments; the
// input thread converts and publishes the complete three-device snapshot. An
// even revision observed twice makes all alternatives coherent.
class RuntimeBindingPublication {
public:
    void publish(const RuntimeBindingSnapshot& snapshot) noexcept {
        revision_.fetch_add(1, std::memory_order_acq_rel);
        for (std::size_t index = 0;
             index < kRuntimeBindingSlotCount; ++index) {
            physical_ids_[index].store(
                snapshot.slots[index].physical_id,
                std::memory_order_relaxed);
            analog_[index].store(
                snapshot.slots[index].analog,
                std::memory_order_relaxed);
        }
        revision_.fetch_add(1, std::memory_order_release);
    }

    // The process-wide input hook must never wait on a preempted UI writer.
    // Skip this frame when publication is in progress or changes while read.
    [[nodiscard]] bool try_load(
        VersionedRuntimeBindingSnapshot& result) const noexcept {
        result.revision = revision_.load(std::memory_order_acquire);
        if ((result.revision & 1u) != 0) return false;
        for (std::size_t index = 0;
             index < kRuntimeBindingSlotCount; ++index) {
            result.value.slots[index].physical_id =
                physical_ids_[index].load(std::memory_order_relaxed);
            result.value.slots[index].analog =
                analog_[index].load(std::memory_order_relaxed);
        }
        const std::uint32_t confirmed_revision =
            revision_.load(std::memory_order_acquire);
        return result.revision == confirmed_revision &&
            (confirmed_revision & 1u) == 0;
    }

    [[nodiscard]] VersionedRuntimeBindingSnapshot load() const noexcept {
        VersionedRuntimeBindingSnapshot result{};
        while (!try_load(result)) {
        }
        return result;
    }

    [[nodiscard]] std::uint32_t revision() const noexcept {
        return revision_.load(std::memory_order_acquire);
    }

private:
    // Start at a nonzero even revision so zero remains the edge tracker's
    // "no deferred snapshot" sentinel.
    std::atomic<std::uint32_t> revision_{2};
    std::array<std::atomic<std::int32_t>, kRuntimeBindingSlotCount>
        physical_ids_{std::atomic<std::int32_t>{-1},
                      std::atomic<std::int32_t>{-1},
                      std::atomic<std::int32_t>{-1}};
    std::array<std::atomic<std::uint8_t>, kRuntimeBindingSlotCount>
        analog_{std::atomic<std::uint8_t>{0},
                std::atomic<std::uint8_t>{0},
                std::atomic<std::uint8_t>{0}};
};

// Input-thread-only release/edge state. Each alternative arms independently,
// but all rising edges in one frame coalesce into one logical action. The
// candidate is returned on the following sample so a native editor created
// later in the activation frame can suppress it safely.
class RuntimeBindingEdgeTracker {
public:
    [[nodiscard]] RuntimeBindingDeviceMask sample_mask(
        const VersionedRuntimeBindingSnapshot& snapshot,
        const std::array<bool, kRuntimeBindingSlotCount>& active,
        bool suppressed = false,
        bool queryable = true) noexcept {
        if (snapshot.revision != seen_revision_) {
            seen_revision_ = snapshot.revision;
            reset_observation();
        }

        if (suppressed || !queryable) {
            reset_observation();
            return false;
        }

        const RuntimeBindingDeviceMask dispatch =
            deferred_revision_ == snapshot.revision
            ? deferred_device_mask_
            : 0;
        deferred_device_mask_ = 0;
        deferred_revision_ = 0;

        RuntimeBindingDeviceMask pressed = 0;
        for (std::size_t index = 0;
             index < kRuntimeBindingSlotCount; ++index) {
            SlotState& state = slots_[index];
            if (!snapshot.value.slots[index].bound()) {
                state = {};
                continue;
            }

            if (!state.armed) {
                state.previous_active = active[index];
                if (!active[index]) state.armed = true;
                continue;
            }

            if (active[index] && !state.previous_active) {
                pressed |= runtime_binding_device_bit(index);
            }
            state.previous_active = active[index];
        }

        if (pressed != 0) {
            deferred_device_mask_ = pressed;
            deferred_revision_ = snapshot.revision;
        }
        return dispatch;
    }

    // Compatibility convenience for callers that only need a logical edge.
    [[nodiscard]] bool sample(
        const VersionedRuntimeBindingSnapshot& snapshot,
        const std::array<bool, kRuntimeBindingSlotCount>& active,
        bool suppressed = false,
        bool queryable = true) noexcept {
        return sample_mask(snapshot, active, suppressed, queryable) != 0;
    }

    void reset() noexcept {
        seen_revision_ = std::numeric_limits<std::uint32_t>::max();
        reset_observation();
    }

private:
    struct SlotState {
        bool armed{};
        bool previous_active{};
    };

    void reset_observation() noexcept {
        slots_ = {};
        deferred_device_mask_ = 0;
        deferred_revision_ = 0;
    }

    std::array<SlotState, kRuntimeBindingSlotCount> slots_{};
    std::uint32_t seen_revision_{
        std::numeric_limits<std::uint32_t>::max()};
    RuntimeBindingDeviceMask deferred_device_mask_{};
    std::uint32_t deferred_revision_{};
};

} // namespace erui::native
