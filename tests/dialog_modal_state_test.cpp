#include "dialog_modal_state.hpp"
#include "dialog_input_gate.hpp"

#include "test_assertions.hpp"

int main() {
    using erui::native::should_suppress_native_back;
    ERUI_TEST_CHECK(!should_suppress_native_back(false, false, 3));
    ERUI_TEST_CHECK(should_suppress_native_back(true, false, 3));
    ERUI_TEST_CHECK(!should_suppress_native_back(true, true, 3));
    ERUI_TEST_CHECK(!should_suppress_native_back(true, false, 2));

    erui::native::DialogModalState state{};
    int owner_a{};
    int owner_b{};
    int job_a{};
    int job_b{};

    ERUI_TEST_CHECK(!state.active());
    ERUI_TEST_CHECK(!state.owns(&owner_a, &job_a));
    ERUI_TEST_CHECK(state.reconcile(&owner_a, nullptr).job == nullptr);

    state.publish(&owner_a, &job_a);
    ERUI_TEST_CHECK(state.active());
    ERUI_TEST_CHECK(state.owns(&owner_a, &job_a));
    ERUI_TEST_CHECK(!state.owns(&owner_a, &job_b));
    ERUI_TEST_CHECK(!state.owns(&owner_b, &job_a));
    ERUI_TEST_CHECK(state.reconcile(&owner_a, &job_a).job == nullptr);
    ERUI_TEST_CHECK(state.active());

    const auto dismissed = state.reconcile(&owner_a, nullptr);
    ERUI_TEST_CHECK(dismissed.job == &job_a);
    ERUI_TEST_CHECK(dismissed.loss == erui::native::DialogModalLoss::dismissed);
    ERUI_TEST_CHECK(!state.active());
    ERUI_TEST_CHECK(state.reconcile(&owner_a, nullptr).job == nullptr);

    state.publish(&owner_a, &job_b);
    const auto replaced_owner = state.reconcile(&owner_b, &job_b);
    ERUI_TEST_CHECK(replaced_owner.job == &job_b);
    ERUI_TEST_CHECK(replaced_owner.loss ==
        erui::native::DialogModalLoss::ownership_lost);
    ERUI_TEST_CHECK(!state.active());

    state.publish(&owner_a, &job_a);
    const auto replaced_job = state.reconcile(&owner_a, &job_b);
    ERUI_TEST_CHECK(replaced_job.job == &job_a);
    ERUI_TEST_CHECK(replaced_job.loss ==
        erui::native::DialogModalLoss::ownership_lost);
    ERUI_TEST_CHECK(!state.active());

    state.publish(&owner_b, &job_a);
    state.reset();
    ERUI_TEST_CHECK(!state.active());
    ERUI_TEST_CHECK(!state.owns(&owner_b, &job_a));
    return 0;
}
