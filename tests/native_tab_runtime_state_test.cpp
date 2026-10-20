#include "native_tab_runtime_state.hpp"
#include "native_tab_slot_growth_policy.hpp"

#include "test_assertions.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

using erui::detail::NativeTabRuntimeEntry;
using erui::detail::NativeTabLiveGeometry;
using erui::detail::NativeTabPhysicalLayer;
using erui::detail::NativeTabRuntimeState;
using erui::detail::NativeTabSlotGrowthStatus;
using erui::detail::invalid_native_tab_runtime_index;
using erui::detail::native_tab_physical_host_capacity;
using erui::detail::native_tab_physical_layer_count;
using erui::detail::native_tab_physical_slot_capacity;
using erui::detail::native_tab_active_slot_target;
using erui::detail::validate_native_tab_live_geometry;
using erui::detail::plan_native_tab_slot_growth;

void test_slot_growth_plan_requires_symmetric_contiguous_cells() {
    constexpr std::array<bool, 3> stock{true, false, false};
    constexpr std::array<bool, 3> nine{true, true, false};
    constexpr std::array<bool, 3> ten{true, true, true};

    auto plan = plan_native_tab_slot_growth(stock, stock, 8);
    ERUI_TEST_CHECK(plan.minimum_satisfied());
    ERUI_TEST_CHECK(plan.discovered_slot_count == 8);

    plan = plan_native_tab_slot_growth(nine, nine, 8);
    ERUI_TEST_CHECK(plan.minimum_satisfied());
    ERUI_TEST_CHECK(plan.discovered_slot_count == 9);
    plan = plan_native_tab_slot_growth(nine, nine, 9);
    ERUI_TEST_CHECK(plan.minimum_satisfied());
    plan = plan_native_tab_slot_growth(nine, nine, 10);
    ERUI_TEST_CHECK(plan.structure_exact());
    ERUI_TEST_CHECK(!plan.minimum_satisfied());
    ERUI_TEST_CHECK(plan.status == NativeTabSlotGrowthStatus::below_minimum);

    for (const std::size_t minimum : {8U, 9U, 10U}) {
        plan = plan_native_tab_slot_growth(ten, ten, minimum);
        ERUI_TEST_CHECK(plan.minimum_satisfied());
        ERUI_TEST_CHECK(plan.discovered_slot_count == 10);
    }

    plan = plan_native_tab_slot_growth(stock, nine, 8);
    ERUI_TEST_CHECK(
        plan.status == NativeTabSlotGrowthStatus::asymmetric_layers);
    ERUI_TEST_CHECK(!plan.structure_exact());

    constexpr std::array<bool, 3> missing_eight{false, false, false};
    plan = plan_native_tab_slot_growth(missing_eight, missing_eight, 8);
    ERUI_TEST_CHECK(plan.status ==
        NativeTabSlotGrowthStatus::missing_stock_trailing_cell);

    constexpr std::array<bool, 3> hole{true, false, true};
    plan = plan_native_tab_slot_growth(hole, hole, 8);
    ERUI_TEST_CHECK(
        plan.status == NativeTabSlotGrowthStatus::non_contiguous_cells);

    plan = plan_native_tab_slot_growth(stock, stock, 7);
    ERUI_TEST_CHECK(
        plan.status == NativeTabSlotGrowthStatus::invalid_minimum);
    plan = plan_native_tab_slot_growth(ten, ten, 11);
    ERUI_TEST_CHECK(
        plan.status == NativeTabSlotGrowthStatus::invalid_minimum);
}

void test_slot_growth_active_count_never_exceeds_real_logical_routes() {
    ERUI_TEST_CHECK(native_tab_active_slot_target(8, 9) == 8);
    ERUI_TEST_CHECK(native_tab_active_slot_target(10, 8) == 8);
    ERUI_TEST_CHECK(native_tab_active_slot_target(9, 9) == 9);
    ERUI_TEST_CHECK(native_tab_active_slot_target(10, 9) == 9);
    ERUI_TEST_CHECK(native_tab_active_slot_target(10, 10) == 10);
    ERUI_TEST_CHECK(native_tab_active_slot_target(10, 58) == 10);

    ERUI_TEST_CHECK(native_tab_active_slot_target(7, 9) == 0);
    ERUI_TEST_CHECK(native_tab_active_slot_target(11, 11) == 0);
    ERUI_TEST_CHECK(native_tab_active_slot_target(10, 7) == 0);
}

void test_live_geometry_requires_one_dimensional_trailing_capacity() {
    NativeTabLiveGeometry geometry{};
    ERUI_TEST_CHECK(validate_native_tab_live_geometry(
        8, 8, 0, 9, 1, geometry));
    ERUI_TEST_CHECK(geometry.active_slot_count == 8);
    ERUI_TEST_CHECK(geometry.authored_cell_count == 9);
    ERUI_TEST_CHECK(geometry.selected_slot == 0);

    ERUI_TEST_CHECK(validate_native_tab_live_geometry(
        9, 9, 8, 1, 10, geometry));
    ERUI_TEST_CHECK(geometry.active_slot_count == 9);
    ERUI_TEST_CHECK(geometry.authored_cell_count == 10);
    ERUI_TEST_CHECK(geometry.selected_slot == 8);

    // Extra third-party-authored cells may remain dormant beyond the active
    // native count; the host never needs to delete them.
    ERUI_TEST_CHECK(validate_native_tab_live_geometry(
        9, 9, 8, 1, 11, geometry));
    ERUI_TEST_CHECK(geometry.active_slot_count == 9);
    ERUI_TEST_CHECK(geometry.authored_cell_count == 11);

    // The exact active+1 boundary is accepted, but a strip without its
    // authored trailing cell is not.
    ERUI_TEST_CHECK(validate_native_tab_live_geometry(
        10, 10, 4, 1, 11, geometry));
    ERUI_TEST_CHECK(!validate_native_tab_live_geometry(
        10, 10, 4, 1, 10, geometry));

    // A product with enough cells is not sufficient: the Configuration tab
    // selector is a one-dimensional strip, never an arbitrary grid.
    ERUI_TEST_CHECK(!validate_native_tab_live_geometry(
        9, 9, 0, 2, 5, geometry));
    ERUI_TEST_CHECK(!validate_native_tab_live_geometry(
        8, 8, 0, 32, 64, geometry));

    ERUI_TEST_CHECK(!validate_native_tab_live_geometry(
        8, 8, -1, 9, 1, geometry));
    ERUI_TEST_CHECK(!validate_native_tab_live_geometry(
        8, 8, 8, 9, 1, geometry));
    ERUI_TEST_CHECK(!validate_native_tab_live_geometry(
        9, 8, 0, 9, 1, geometry));
    ERUI_TEST_CHECK(!validate_native_tab_live_geometry(
        7, 7, 0, 8, 1, geometry));
    ERUI_TEST_CHECK(!validate_native_tab_live_geometry(
        11, 11, 0, 12, 1, geometry));
    ERUI_TEST_CHECK(!validate_native_tab_live_geometry(
        8, 8, 0, 33, 1, geometry));
    ERUI_TEST_CHECK(!validate_native_tab_live_geometry(
        8, 8, 0, 1, 65, geometry));
}

void test_allocates_only_actual_tab_count() {
    constexpr std::array<std::size_t, 5> counts{0, 1, 3, 50, 1024};
    for (const std::size_t count : counts) {
        auto runtime = NativeTabRuntimeState::create(count);
        ERUI_TEST_CHECK(runtime);
        ERUI_TEST_CHECK(runtime->size() == count);
        ERUI_TEST_CHECK(runtime->empty() == (count == 0));
        ERUI_TEST_CHECK(runtime->active_physical_slot_count() == 0);
        ERUI_TEST_CHECK(runtime->active_physical_slots().empty());
        ERUI_TEST_CHECK(runtime->flattened_physical_host_count() == 0);

        for (std::size_t index = 0; index < count; ++index) {
            const NativeTabRuntimeEntry* const entry = runtime->tab(index);
            ERUI_TEST_CHECK(entry);
            ERUI_TEST_CHECK(entry->index() == index);
        }
        ERUI_TEST_CHECK(runtime->tab(count) == nullptr);
    }

    // The loader-facing factory must fail closed rather than throw when a
    // nonsensical count cannot possibly be represented by its vector.
    ERUI_TEST_CHECK(!NativeTabRuntimeState::create(
        std::numeric_limits<std::size_t>::max()));
}

void test_entry_identity_is_stable_and_icon_state_is_independent() {
    auto runtime = NativeTabRuntimeState::create(3);
    ERUI_TEST_CHECK(runtime);
    ERUI_TEST_CHECK(runtime->begin_physical_epoch(8));

    std::array<NativeTabRuntimeEntry*, 3> identities{
        runtime->tab(0), runtime->tab(1), runtime->tab(2)};
    for (std::size_t index = 0; index < identities.size(); ++index) {
        auto& icon = identities[index]->icon();
        icon.image.store(reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(0x1000U + index * 0x100U)));
        icon.resource.store(reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(0x2000U + index * 0x100U)));
        icon.checksum.store(0xA0U + index);
        icon.ready.store(index != 1);
    }

    ERUI_TEST_CHECK(runtime->set_active_host(0, 2));
    ERUI_TEST_CHECK(runtime->set_staged_host(1, 1));
    ERUI_TEST_CHECK(runtime->set_retirement_debt(2, 4));

    for (std::size_t index = 0; index < identities.size(); ++index) {
        ERUI_TEST_CHECK(runtime->tab(index) == identities[index]);
        const auto& icon = runtime->tab(index)->icon();
        ERUI_TEST_CHECK(icon.image.load() == reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(0x1000U + index * 0x100U)));
        ERUI_TEST_CHECK(icon.resource.load() == reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(0x2000U + index * 0x100U)));
        ERUI_TEST_CHECK(icon.checksum.load() == 0xA0U + index);
        ERUI_TEST_CHECK(icon.ready.load() == (index != 1));
    }

    runtime->tab(1)->icon().checksum.store(0xDEADBEEFU);
    runtime->tab(1)->icon().ready.store(true);
    ERUI_TEST_CHECK(runtime->tab(0)->icon().checksum.load() == 0xA0U);
    ERUI_TEST_CHECK(runtime->tab(2)->icon().checksum.load() == 0xA2U);
    ERUI_TEST_CHECK(runtime->tab(0)->icon().ready.load());
    ERUI_TEST_CHECK(runtime->tab(2)->icon().ready.load());
}

void test_physical_epochs_are_runtime_sized_and_layer_safe() {
    constexpr std::size_t high_index = 1023;
    static_assert(high_index > std::numeric_limits<std::uint8_t>::max());
    auto runtime = NativeTabRuntimeState::create(high_index + 1);
    ERUI_TEST_CHECK(runtime);

    ERUI_TEST_CHECK(!runtime->begin_physical_epoch(0));
    ERUI_TEST_CHECK(!runtime->begin_physical_epoch(
        native_tab_physical_slot_capacity + 1));

    constexpr std::array<std::size_t, 3> epoch_sizes{8, 9, 10};
    for (const std::size_t slot_count : epoch_sizes) {
        ERUI_TEST_CHECK(runtime->begin_physical_epoch(slot_count));
        ERUI_TEST_CHECK(!runtime->begin_physical_epoch(slot_count));
        ERUI_TEST_CHECK(
            runtime->active_physical_slot_count() == slot_count);
        ERUI_TEST_CHECK(
            runtime->active_physical_slots().size() == slot_count);
        ERUI_TEST_CHECK(
            runtime->flattened_physical_host_count() == slot_count * 2U);

        for (std::size_t slot = 0; slot < slot_count; ++slot) {
            const std::size_t front_tab = slot == 0 ? high_index : slot;
            const std::size_t back_tab = 100U + slot;
            ERUI_TEST_CHECK(runtime->set_active_host(
                slot, NativeTabPhysicalLayer::front, front_tab));
            ERUI_TEST_CHECK(runtime->set_active_host(
                slot, NativeTabPhysicalLayer::back, back_tab));
            ERUI_TEST_CHECK(runtime->set_staged_host(
                slot, NativeTabPhysicalLayer::front, back_tab));
            ERUI_TEST_CHECK(runtime->set_staged_host(
                slot, NativeTabPhysicalLayer::back, front_tab));

            const auto* const front = runtime->host(
                slot, NativeTabPhysicalLayer::front);
            const auto* const back = runtime->host(
                slot, NativeTabPhysicalLayer::back);
            ERUI_TEST_CHECK(front);
            ERUI_TEST_CHECK(back);
            ERUI_TEST_CHECK(front != back);
            ERUI_TEST_CHECK(front->active_tab_index.load() == front_tab);
            ERUI_TEST_CHECK(back->active_tab_index.load() == back_tab);
            ERUI_TEST_CHECK(front->staged_tab_index.load() == back_tab);
            ERUI_TEST_CHECK(back->staged_tab_index.load() == front_tab);

            // The compatibility view is dynamic: all front hosts precede all
            // back hosts for the current epoch's actual slot count.
            ERUI_TEST_CHECK(runtime->host(slot) == front);
            ERUI_TEST_CHECK(runtime->host(slot_count + slot) == back);
        }

        ERUI_TEST_CHECK(runtime->active_host_count(high_index) == 1);
        ERUI_TEST_CHECK(runtime->staged_host_count(high_index) == 1);
        ERUI_TEST_CHECK(runtime->total_host_count(high_index) == 2);
        ERUI_TEST_CHECK(runtime->physical_slot(slot_count) == nullptr);
        ERUI_TEST_CHECK(runtime->host(
            slot_count, NativeTabPhysicalLayer::front) == nullptr);
        ERUI_TEST_CHECK(runtime->host(slot_count * 2U) == nullptr);
        ERUI_TEST_CHECK(!runtime->set_active_host(
            slot_count, NativeTabPhysicalLayer::front, 0));
        ERUI_TEST_CHECK(!runtime->set_staged_host(
            0, NativeTabPhysicalLayer::front, runtime->size()));
        ERUI_TEST_CHECK(!runtime->set_active_host(
            0,
            static_cast<NativeTabPhysicalLayer>(0xFF),
            0));
        ERUI_TEST_CHECK(!runtime->clear_active_host(
            slot_count, NativeTabPhysicalLayer::back));

        runtime->reset_epoch_tracking();
        ERUI_TEST_CHECK(runtime->active_physical_slot_count() == 0);
        ERUI_TEST_CHECK(runtime->active_physical_slots().empty());
        ERUI_TEST_CHECK(runtime->host(0) == nullptr);
        ERUI_TEST_CHECK(runtime->host(
            0, NativeTabPhysicalLayer::front) == nullptr);
    }
}

void test_epoch_shrink_and_growth_do_not_resurrect_host_ownership() {
    auto runtime = NativeTabRuntimeState::create(32);
    ERUI_TEST_CHECK(runtime);

    ERUI_TEST_CHECK(runtime->begin_physical_epoch(10));
    ERUI_TEST_CHECK(runtime->set_active_host(
        9, NativeTabPhysicalLayer::back, 31));
    ERUI_TEST_CHECK(runtime->set_staged_host(
        8, NativeTabPhysicalLayer::front, 30));
    runtime->reset_epoch_tracking();

    ERUI_TEST_CHECK(runtime->begin_physical_epoch(8));
    for (std::size_t slot = 0; slot < 8; ++slot) {
        for (const NativeTabPhysicalLayer layer : {
                 NativeTabPhysicalLayer::front,
                 NativeTabPhysicalLayer::back}) {
            const auto* const host = runtime->host(slot, layer);
            ERUI_TEST_CHECK(host);
            ERUI_TEST_CHECK(!host->has_active_tab());
            ERUI_TEST_CHECK(!host->has_staged_tab());
        }
    }
    ERUI_TEST_CHECK(runtime->physical_slot(8) == nullptr);
    runtime->reset_epoch_tracking();

    ERUI_TEST_CHECK(runtime->begin_physical_epoch(10));
    ERUI_TEST_CHECK(!runtime->host(
        9, NativeTabPhysicalLayer::back)->has_active_tab());
    ERUI_TEST_CHECK(!runtime->host(
        8, NativeTabPhysicalLayer::front)->has_staged_tab());
}

void test_retirement_worklist_is_sparse_and_duplicate_safe() {
    auto runtime = NativeTabRuntimeState::create(1024);
    ERUI_TEST_CHECK(runtime);

    ERUI_TEST_CHECK(runtime->set_retirement_debt(0, 0));
    ERUI_TEST_CHECK(runtime->retiring_tab_indices().empty());
    ERUI_TEST_CHECK(runtime->set_retirement_debt(1, 3));
    ERUI_TEST_CHECK(runtime->set_retirement_debt(17, 5));
    ERUI_TEST_CHECK(runtime->set_retirement_debt(1023, 7));
    ERUI_TEST_CHECK(runtime->set_retirement_debt(1, 2));
    ERUI_TEST_CHECK(runtime->retiring_tab_indices().size() == 3);
    ERUI_TEST_CHECK(runtime->retiring_tab_indices()[0] == 1);
    ERUI_TEST_CHECK(runtime->retiring_tab_indices()[1] == 17);
    ERUI_TEST_CHECK(runtime->retiring_tab_indices()[2] == 1023);
    ERUI_TEST_CHECK(runtime->tab(1)->retirement_debt() == 2);
    ERUI_TEST_CHECK(runtime->tab(17)->retirement_debt() == 5);
    ERUI_TEST_CHECK(runtime->tab(1023)->retirement_debt() == 7);

    // Removing a middle element exercises swap-and-pop and the reverse
    // position update on the tab moved into its place.
    ERUI_TEST_CHECK(runtime->set_retirement_debt(17, 0));
    ERUI_TEST_CHECK(runtime->retiring_tab_indices().size() == 2);
    ERUI_TEST_CHECK(runtime->retiring_tab_indices()[0] == 1);
    ERUI_TEST_CHECK(runtime->retiring_tab_indices()[1] == 1023);
    ERUI_TEST_CHECK(runtime->tab(17)->retirement_debt() == 0);

    ERUI_TEST_CHECK(runtime->set_retirement_debt(1, 0));
    ERUI_TEST_CHECK(runtime->retiring_tab_indices().size() == 1);
    ERUI_TEST_CHECK(runtime->retiring_tab_indices().front() == 1023);
    ERUI_TEST_CHECK(runtime->tab(1)->retirement_debt() == 0);
    ERUI_TEST_CHECK(runtime->tab(1023)->retirement_debt() == 7);

    ERUI_TEST_CHECK(runtime->set_retirement_debt(1023, 0));
    ERUI_TEST_CHECK(runtime->retiring_tab_indices().empty());
    ERUI_TEST_CHECK(!runtime->set_retirement_debt(runtime->size(), 1));
    ERUI_TEST_CHECK(runtime->retiring_tab_indices().empty());
}

void test_icon_publication_release_acquire_contract() {
    auto runtime = NativeTabRuntimeState::create(1024);
    ERUI_TEST_CHECK(runtime);
    auto& icon = runtime->tab(1023)->icon();
    constexpr std::uint64_t iterations = 4096;
    std::atomic_bool mismatch{false};

    std::thread producer([&]() noexcept {
        for (std::uint64_t generation = 1; generation <= iterations;
             ++generation) {
            while (icon.ready.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            icon.image.store(reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(generation * 2U + 1U)),
                std::memory_order_relaxed);
            icon.resource.store(reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(generation * 2U + 2U)),
                std::memory_order_relaxed);
            icon.checksum.store(generation, std::memory_order_relaxed);
            icon.ready.store(true, std::memory_order_release);
        }
    });
    std::thread consumer([&]() noexcept {
        for (std::uint64_t generation = 1; generation <= iterations;
             ++generation) {
            while (!icon.ready.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            const std::uint64_t checksum =
                icon.checksum.load(std::memory_order_relaxed);
            const auto image = reinterpret_cast<std::uintptr_t>(
                icon.image.load(std::memory_order_relaxed));
            const auto resource = reinterpret_cast<std::uintptr_t>(
                icon.resource.load(std::memory_order_relaxed));
            if (checksum != generation || image != generation * 2U + 1U ||
                resource != generation * 2U + 2U) {
                mismatch.store(true, std::memory_order_relaxed);
            }
            icon.ready.store(false, std::memory_order_release);
        }
    });
    producer.join();
    consumer.join();
    ERUI_TEST_CHECK(!mismatch.load(std::memory_order_relaxed));
}

void test_epoch_reset_preserves_retained_icon_roots() {
    auto runtime = NativeTabRuntimeState::create(3);
    ERUI_TEST_CHECK(runtime);
    ERUI_TEST_CHECK(runtime->begin_physical_epoch(10));

    auto& icon = runtime->tab(2)->icon();
    icon.image.store(reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(0x1234)));
    icon.resource.store(reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(0x5678)));
    icon.checksum.store(0xCAFEBABEULL);
    icon.ready.store(true);
    ERUI_TEST_CHECK(runtime->set_active_host(0, 2));
    ERUI_TEST_CHECK(runtime->set_staged_host(
        9, NativeTabPhysicalLayer::back, 2));
    ERUI_TEST_CHECK(runtime->set_retirement_debt(2, 2));

    runtime->reset_epoch_tracking();

    ERUI_TEST_CHECK(runtime->retiring_tab_indices().empty());
    ERUI_TEST_CHECK(runtime->tab(2)->retirement_debt() == 0);
    ERUI_TEST_CHECK(runtime->active_physical_slot_count() == 0);
    ERUI_TEST_CHECK(runtime->active_physical_slots().empty());
    ERUI_TEST_CHECK(runtime->host(0) == nullptr);
    ERUI_TEST_CHECK(icon.image.load() == reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(0x1234)));
    ERUI_TEST_CHECK(icon.resource.load() == reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(0x5678)));
    ERUI_TEST_CHECK(icon.checksum.load() == 0xCAFEBABEULL);
    ERUI_TEST_CHECK(icon.ready.load());
}

} // namespace

static_assert(native_tab_physical_slot_capacity == 10);
static_assert(native_tab_physical_layer_count == 2);
static_assert(native_tab_physical_host_capacity == 20);
static_assert(!std::is_copy_constructible_v<NativeTabRuntimeEntry>);
static_assert(!std::is_move_constructible_v<NativeTabRuntimeEntry>);
static_assert(!std::is_copy_constructible_v<NativeTabRuntimeState>);
static_assert(!std::is_move_constructible_v<NativeTabRuntimeState>);
static_assert(noexcept(NativeTabRuntimeState::create(0)));

int main() {
    test_slot_growth_plan_requires_symmetric_contiguous_cells();
    test_slot_growth_active_count_never_exceeds_real_logical_routes();
    test_live_geometry_requires_one_dimensional_trailing_capacity();
    test_allocates_only_actual_tab_count();
    test_entry_identity_is_stable_and_icon_state_is_independent();
    test_physical_epochs_are_runtime_sized_and_layer_safe();
    test_epoch_shrink_and_growth_do_not_resurrect_host_ownership();
    test_retirement_worklist_is_sparse_and_duplicate_safe();
    test_icon_publication_release_acquire_contract();
    test_epoch_reset_preserves_retained_icon_roots();
    return 0;
}
