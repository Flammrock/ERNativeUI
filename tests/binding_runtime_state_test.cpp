#include "binding_runtime_state.hpp"

#include "test_assertions.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <thread>

namespace {

using erui::native::RuntimeBindingEdgeTracker;
using erui::native::RuntimeBindingDeviceMask;
using erui::native::RuntimeBindingPublication;
using erui::native::RuntimeBindingSlot;
using erui::native::RuntimeBindingSlotIndex;
using erui::native::RuntimeBindingSnapshot;
using erui::native::VersionedRuntimeBindingSnapshot;
using erui::native::kRuntimeBindingSlotCount;
using erui::native::runtime_binding_device_bit;

constexpr std::size_t slot(RuntimeBindingSlotIndex index) noexcept {
    return static_cast<std::size_t>(index);
}

constexpr RuntimeBindingSlot kController{51, 0};
constexpr RuntimeBindingSlot kKeyboard{101, 0};
constexpr RuntimeBindingSlot kMouse{202, 1};
constexpr std::array<bool, kRuntimeBindingSlotCount> kReleased{};

RuntimeBindingSnapshot all_bound() {
    RuntimeBindingSnapshot value{};
    value.slots[slot(RuntimeBindingSlotIndex::controller)] = kController;
    value.slots[slot(RuntimeBindingSlotIndex::keyboard)] = kKeyboard;
    value.slots[slot(RuntimeBindingSlotIndex::mouse)] = kMouse;
    return value;
}

void test_full_snapshot_publication_and_independent_clear() {
    RuntimeBindingPublication publication{};
    const auto empty = publication.load();
    ERUI_TEST_CHECK((empty.revision & 1u) == 0);
    ERUI_TEST_CHECK(!empty.value.slots[0].bound());
    ERUI_TEST_CHECK(!empty.value.slots[1].bound());
    ERUI_TEST_CHECK(!empty.value.slots[2].bound());

    RuntimeBindingSnapshot keyboard_only{};
    keyboard_only.slots[slot(RuntimeBindingSlotIndex::keyboard)] = kKeyboard;
    publication.publish(keyboard_only);
    const auto first = publication.load();
    ERUI_TEST_CHECK(first.revision != empty.revision);
    ERUI_TEST_CHECK(first.value == keyboard_only);

    // Capturing mouse republishes the complete shared BindingValue. It must
    // add the second alternative without replacing keyboard at runtime.
    RuntimeBindingSnapshot keyboard_mouse = keyboard_only;
    keyboard_mouse.slots[slot(RuntimeBindingSlotIndex::mouse)] = kMouse;
    publication.publish(keyboard_mouse);
    const auto second = publication.load();
    ERUI_TEST_CHECK(second.revision != first.revision);
    ERUI_TEST_CHECK(second.value == keyboard_mouse);

    // Capturing controller republishes the same value again and must preserve
    // both assignments made on the other native configuration screen.
    const RuntimeBindingSnapshot all = all_bound();
    publication.publish(all);
    const auto third = publication.load();
    ERUI_TEST_CHECK(third.revision != second.revision);
    ERUI_TEST_CHECK(third.value == all);

    // Clearing keyboard preserves the controller and mouse alternatives.
    RuntimeBindingSnapshot controller_mouse = all;
    controller_mouse.slots[slot(RuntimeBindingSlotIndex::keyboard)] = {};
    publication.publish(controller_mouse);
    const auto fourth = publication.load();
    ERUI_TEST_CHECK(fourth.revision != third.revision);
    ERUI_TEST_CHECK(
        fourth.value.slots[slot(RuntimeBindingSlotIndex::controller)] ==
        kController);
    ERUI_TEST_CHECK(
        !fourth.value.slots[slot(RuntimeBindingSlotIndex::keyboard)].bound());
    ERUI_TEST_CHECK(
        fourth.value.slots[slot(RuntimeBindingSlotIndex::mouse)] == kMouse);

    // Clearing controller preserves both keyboard/mouse alternatives.
    RuntimeBindingSnapshot without_controller = all;
    without_controller.slots[slot(RuntimeBindingSlotIndex::controller)] = {};
    publication.publish(without_controller);
    const auto fifth = publication.load();
    ERUI_TEST_CHECK(fifth.revision != fourth.revision);
    ERUI_TEST_CHECK(
        !fifth.value.slots[slot(RuntimeBindingSlotIndex::controller)].bound());
    ERUI_TEST_CHECK(
        fifth.value.slots[slot(RuntimeBindingSlotIndex::keyboard)] ==
        kKeyboard);
    ERUI_TEST_CHECK(
        fifth.value.slots[slot(RuntimeBindingSlotIndex::mouse)] == kMouse);

    // Clearing mouse preserves both controller/keyboard alternatives.
    RuntimeBindingSnapshot controller_keyboard = all;
    controller_keyboard.slots[slot(RuntimeBindingSlotIndex::mouse)] = {};
    publication.publish(controller_keyboard);
    const auto sixth = publication.load();
    ERUI_TEST_CHECK(sixth.revision != fifth.revision);
    ERUI_TEST_CHECK(
        sixth.value.slots[slot(RuntimeBindingSlotIndex::controller)] ==
        kController);
    ERUI_TEST_CHECK(
        sixth.value.slots[slot(RuntimeBindingSlotIndex::keyboard)] ==
        kKeyboard);
    ERUI_TEST_CHECK(
        !sixth.value.slots[slot(RuntimeBindingSlotIndex::mouse)].bound());
}

void test_publication_never_exposes_a_torn_triple() {
    RuntimeBindingPublication publication{};
    RuntimeBindingSnapshot first{};
    first.slots = {
        RuntimeBindingSlot{11, 0},
        RuntimeBindingSlot{12, 1},
        RuntimeBindingSlot{13, 0}};
    RuntimeBindingSnapshot second{};
    second.slots = {
        RuntimeBindingSlot{21, 1},
        RuntimeBindingSlot{22, 0},
        RuntimeBindingSlot{23, 1}};
    publication.publish(first);

    std::atomic_bool writer_done{};
    std::thread writer{[&]() {
        for (int iteration = 0; iteration < 100'000; ++iteration) {
            publication.publish((iteration & 1) != 0 ? first : second);
        }
        writer_done.store(true, std::memory_order_release);
    }};

    std::size_t observations = 0;
    while (!writer_done.load(std::memory_order_acquire) ||
           observations < 10'000) {
        const RuntimeBindingSnapshot observed = publication.load().value;
        ERUI_TEST_CHECK(observed == first || observed == second);
        ++observations;
    }
    writer.join();
}

void test_independent_release_to_arm_and_edges() {
    RuntimeBindingPublication publication{};
    publication.publish(all_bound());
    const VersionedRuntimeBindingSnapshot snapshot = publication.load();
    RuntimeBindingEdgeTracker tracker{};

    ERUI_TEST_CHECK(!tracker.sample(snapshot, kReleased));

    ERUI_TEST_CHECK(!tracker.sample(snapshot, {true, false, false}));
    ERUI_TEST_CHECK(tracker.sample(snapshot, {true, false, false}));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, kReleased));

    ERUI_TEST_CHECK(!tracker.sample(snapshot, {false, true, false}));
    ERUI_TEST_CHECK(tracker.sample(snapshot, {false, true, false}));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, kReleased));

    ERUI_TEST_CHECK(!tracker.sample(snapshot, {false, false, true}));
    ERUI_TEST_CHECK(tracker.sample(snapshot, {false, false, true}));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {false, false, true}));
}

void test_same_frame_edges_coalesce() {
    RuntimeBindingPublication publication{};
    publication.publish(all_bound());
    const auto snapshot = publication.load();
    RuntimeBindingEdgeTracker tracker{};

    ERUI_TEST_CHECK(!tracker.sample(snapshot, kReleased));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {true, true, true}));
    ERUI_TEST_CHECK(tracker.sample(snapshot, {true, true, true}));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {true, true, true}));
}

void test_edge_mask_preserves_each_triggering_device() {
    RuntimeBindingPublication publication{};
    publication.publish(all_bound());
    const auto snapshot = publication.load();
    RuntimeBindingEdgeTracker tracker{};

    ERUI_TEST_CHECK(tracker.sample_mask(snapshot, kReleased) == 0);
    ERUI_TEST_CHECK(
        tracker.sample_mask(snapshot, {true, false, true}) == 0);

    const RuntimeBindingDeviceMask expected =
        runtime_binding_device_bit(slot(RuntimeBindingSlotIndex::controller)) |
        runtime_binding_device_bit(slot(RuntimeBindingSlotIndex::mouse));
    ERUI_TEST_CHECK(
        tracker.sample_mask(snapshot, {true, false, true}) == expected);
    ERUI_TEST_CHECK(
        tracker.sample_mask(snapshot, {true, false, true}) == 0);
}

void test_held_alternative_does_not_block_another_device() {
    RuntimeBindingPublication publication{};
    publication.publish(all_bound());
    const auto snapshot = publication.load();
    RuntimeBindingEdgeTracker tracker{};

    // Controller was already held when the snapshot became visible, so it is
    // not armed. Released keyboard/mouse alternatives must still arm and fire.
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {true, false, false}));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {true, true, false}));
    ERUI_TEST_CHECK(tracker.sample(snapshot, {true, true, false}));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {true, false, false}));

    // Once controller is released, it independently arms and can fire too.
    ERUI_TEST_CHECK(!tracker.sample(snapshot, kReleased));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {true, false, false}));
    ERUI_TEST_CHECK(tracker.sample(snapshot, {true, false, false}));
}

void test_revision_invalidates_deferred_edge() {
    RuntimeBindingPublication publication{};
    publication.publish(all_bound());
    RuntimeBindingEdgeTracker tracker{};
    const auto old_snapshot = publication.load();

    ERUI_TEST_CHECK(!tracker.sample(old_snapshot, kReleased));
    ERUI_TEST_CHECK(!tracker.sample(old_snapshot, {true, false, false}));

    RuntimeBindingSnapshot mouse_only{};
    mouse_only.slots[slot(RuntimeBindingSlotIndex::mouse)] = kMouse;
    publication.publish(mouse_only);
    const auto new_snapshot = publication.load();
    ERUI_TEST_CHECK(new_snapshot.revision != old_snapshot.revision);
    ERUI_TEST_CHECK(!tracker.sample(new_snapshot, kReleased));
    ERUI_TEST_CHECK(!tracker.sample(new_snapshot, {false, false, true}));
    ERUI_TEST_CHECK(tracker.sample(new_snapshot, {false, false, true}));
}

void test_suppression_and_query_failure_require_release_again() {
    RuntimeBindingPublication publication{};
    publication.publish(all_bound());
    const auto snapshot = publication.load();
    RuntimeBindingEdgeTracker tracker{};

    ERUI_TEST_CHECK(!tracker.sample(snapshot, kReleased));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {false, true, false}));
    ERUI_TEST_CHECK(
        !tracker.sample(snapshot, {false, true, false}, true));

    // A held key after suppression is disarmed until a release is observed.
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {false, true, false}));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, kReleased));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {false, true, false}));
    ERUI_TEST_CHECK(tracker.sample(snapshot, {false, true, false}));

    tracker.reset();
    ERUI_TEST_CHECK(!tracker.sample(snapshot, kReleased));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {true, false, false}));
    ERUI_TEST_CHECK(
        !tracker.sample(snapshot, {true, false, false}, false, false));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {true, false, false}));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, kReleased));
    ERUI_TEST_CHECK(!tracker.sample(snapshot, {true, false, false}));
    ERUI_TEST_CHECK(tracker.sample(snapshot, {true, false, false}));
}

} // namespace

int main() {
    test_full_snapshot_publication_and_independent_clear();
    test_publication_never_exposes_a_torn_triple();
    test_independent_release_to_arm_and_edges();
    test_same_frame_edges_coalesce();
    test_edge_mask_preserves_each_triggering_device();
    test_held_alternative_does_not_block_another_device();
    test_revision_invalidates_deferred_edge();
    test_suppression_and_query_failure_require_release_again();
    return 0;
}
