#include "tab_icon_retirement_policy.hpp"

#include "test_assertions.hpp"

#include <cstdint>
#include <limits>

namespace {

using erui::detail::Rgba8RetirementLimits;
using erui::detail::Rgba8RetirementPolicyStatus;
using erui::detail::Rgba8RapidEpochHandoffState;
using erui::detail::can_begin_vector_only_epoch_during_rgba8_retirement;
using erui::detail::plan_rgba8_host_removal;
using erui::detail::plan_rgba8_retirement_reconciliation;
using erui::detail::rgba8_resource_identity_preserved;

constexpr Rgba8RetirementLimits kLimits{
    .maximum_live_hosts = 40U,
    .maximum_retirement_debt = 20U,
};

void test_synchronous_host_removal() {
    const auto plan = plan_rgba8_host_removal(
        2U, 0U, 3U, 2U, kLimits);

    ERUI_TEST_CHECK(plan.accepted());
    ERUI_TEST_CHECK(!plan.retirement_pending());
    ERUI_TEST_CHECK(plan.active_hosts_after == 1U);
    ERUI_TEST_CHECK(plan.retirement_debt_after == 0U);
    ERUI_TEST_CHECK(plan.minimum_raw_after == 2U);
    ERUI_TEST_CHECK(plan.maximum_raw_after == 3U);
}

void test_two_delayed_host_removals_accumulate_debt() {
    const auto first = plan_rgba8_host_removal(
        2U, 0U, 3U, 3U, kLimits);
    ERUI_TEST_CHECK(first.accepted());
    ERUI_TEST_CHECK(first.retirement_pending());
    ERUI_TEST_CHECK(first.active_hosts_after == 1U);
    ERUI_TEST_CHECK(first.retirement_debt_after == 1U);

    const auto second = plan_rgba8_host_removal(
        first.active_hosts_after,
        first.retirement_debt_after,
        3U,
        3U,
        kLimits);
    ERUI_TEST_CHECK(second.accepted());
    ERUI_TEST_CHECK(second.retirement_pending());
    ERUI_TEST_CHECK(second.active_hosts_after == 0U);
    ERUI_TEST_CHECK(second.retirement_debt_after == 2U);
    ERUI_TEST_CHECK(second.minimum_raw_after == 1U);
    ERUI_TEST_CHECK(second.maximum_raw_after == 3U);
}

void test_prior_debt_can_retire_during_a_later_host_removal() {
    const auto all_prior_debt_retires = plan_rgba8_host_removal(
        2U, 2U, 5U, 2U, kLimits);
    ERUI_TEST_CHECK(all_prior_debt_retires.accepted());
    ERUI_TEST_CHECK(!all_prior_debt_retires.retirement_pending());
    ERUI_TEST_CHECK(all_prior_debt_retires.active_hosts_after == 1U);
    ERUI_TEST_CHECK(all_prior_debt_retires.retirement_debt_after == 0U);

    const auto one_reference_remains = plan_rgba8_host_removal(
        2U, 2U, 5U, 3U, kLimits);
    ERUI_TEST_CHECK(one_reference_remains.accepted());
    ERUI_TEST_CHECK(one_reference_remains.retirement_pending());
    ERUI_TEST_CHECK(one_reference_remains.active_hosts_after == 1U);
    ERUI_TEST_CHECK(one_reference_remains.retirement_debt_after == 1U);
}

void test_retirement_reconciliation_moves_only_downward() {
    const auto unchanged = plan_rgba8_retirement_reconciliation(
        0U, 2U, 3U, kLimits);
    ERUI_TEST_CHECK(unchanged.accepted());
    ERUI_TEST_CHECK(!unchanged.complete());
    ERUI_TEST_CHECK(unchanged.retirement_debt_after == 2U);

    const auto one_remaining = plan_rgba8_retirement_reconciliation(
        0U, unchanged.retirement_debt_after, 2U, kLimits);
    ERUI_TEST_CHECK(one_remaining.accepted());
    ERUI_TEST_CHECK(!one_remaining.complete());
    ERUI_TEST_CHECK(one_remaining.retirement_debt_after == 1U);

    const auto complete = plan_rgba8_retirement_reconciliation(
        0U, one_remaining.retirement_debt_after, 1U, kLimits);
    ERUI_TEST_CHECK(complete.accepted());
    ERUI_TEST_CHECK(complete.complete());
    ERUI_TEST_CHECK(complete.retirement_debt_after == 0U);
}

void test_resource_identity_not_stable_tab_identity_controls_preservation() {
    const std::uint8_t first_storage{};
    const std::uint8_t second_storage{};
    const void* const first = &first_storage;
    const void* const second = &second_storage;

    ERUI_TEST_CHECK(rgba8_resource_identity_preserved(first, first));
    ERUI_TEST_CHECK(!rgba8_resource_identity_preserved(first, second));
    ERUI_TEST_CHECK(!rgba8_resource_identity_preserved(nullptr, first));
    ERUI_TEST_CHECK(!rgba8_resource_identity_preserved(first, nullptr));
    ERUI_TEST_CHECK(!rgba8_resource_identity_preserved(nullptr, nullptr));
}

void test_retired_and_current_resource_banks_reconcile_independently() {
    // The detached old resource has no live Shape, but two renderer-delayed
    // references. The new immutable resource simultaneously owns four live
    // Shapes and no debt. Both are valid only against their own identity.
    const auto old_bank = plan_rgba8_retirement_reconciliation(
        0U, 2U, 3U, kLimits);
    const auto current_bank = plan_rgba8_retirement_reconciliation(
        4U, 0U, 5U, kLimits);
    ERUI_TEST_CHECK(old_bank.accepted());
    ERUI_TEST_CHECK(!old_bank.complete());
    ERUI_TEST_CHECK(old_bank.retirement_debt_after == 2U);
    ERUI_TEST_CHECK(current_bank.accepted());
    ERUI_TEST_CHECK(current_bank.complete());

    // Sampling either RawImage with the other bank's host/debt arithmetic is
    // rejected. A stable tab index can therefore never substitute for exact
    // resource identity during the overlap.
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        4U, 0U, 3U, kLimits).status ==
        Rgba8RetirementPolicyStatus::observed_raw_below_minimum);
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        0U, 2U, 5U, kLimits).status ==
        Rgba8RetirementPolicyStatus::observed_raw_above_maximum);

    const auto old_bank_complete = plan_rgba8_retirement_reconciliation(
        0U, old_bank.retirement_debt_after, 1U, kLimits);
    const auto current_bank_still_live =
        plan_rgba8_retirement_reconciliation(4U, 0U, 5U, kLimits);
    ERUI_TEST_CHECK(old_bank_complete.complete());
    ERUI_TEST_CHECK(current_bank_still_live.complete());
}

void test_live_overlap_uses_per_resource_host_counts() {
    // A changed icon temporarily has two source-bank Shapes and two
    // target-bank Shapes under the same stable tab identity.  Each immutable
    // RawImage sees only the Shapes bound to its own resource.
    const auto source = plan_rgba8_retirement_reconciliation(
        2U, 0U, 3U, kLimits);
    const auto target = plan_rgba8_retirement_reconciliation(
        2U, 0U, 3U, kLimits);
    ERUI_TEST_CHECK(source.accepted());
    ERUI_TEST_CHECK(target.accepted());

    // Summing both banks by stable asset while sampling either one of their
    // RawImages is invalid.  This is the exact overlap that a live immutable
    // icon replacement must audit by resource identity.
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        4U, 0U, 3U, kLimits).status ==
        Rgba8RetirementPolicyStatus::observed_raw_below_minimum);

    const auto delayed_source_detach = plan_rgba8_host_removal(
        2U, 0U, 3U, 3U, kLimits);
    ERUI_TEST_CHECK(delayed_source_detach.accepted());
    ERUI_TEST_CHECK(delayed_source_detach.active_hosts_after == 1U);
    ERUI_TEST_CHECK(delayed_source_detach.retirement_debt_after == 1U);
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        2U, 0U, 3U, kLimits).accepted());
}

void test_rapid_epoch_handoff_requires_complete_ownership_proof() {
    const Rgba8RapidEpochHandoffState eligible{
        .exact_waiting_for_renderer_retirement = true,
        .physical_geometry_matches = true,
        .active_injected_hosts_empty = true,
        .staged_injected_hosts_empty = true,
        .active_rgba_hosts_empty = true,
        .staged_rgba_hosts_empty = true,
        .active_asset_ledger_empty = true,
        .staged_asset_ledger_empty = true,
        .transaction_active = false,
        .transaction_failed = false,
        .destructive_runtime_failure = false,
    };
    ERUI_TEST_CHECK(
        can_begin_vector_only_epoch_during_rgba8_retirement(eligible));

    Rgba8RapidEpochHandoffState rejected = eligible;
    rejected.exact_waiting_for_renderer_retirement = false;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));

    rejected = eligible;
    rejected.physical_geometry_matches = false;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));

    rejected = eligible;
    rejected.active_injected_hosts_empty = false;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));

    rejected = eligible;
    rejected.staged_injected_hosts_empty = false;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));

    rejected = eligible;
    rejected.active_rgba_hosts_empty = false;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));

    rejected = eligible;
    rejected.staged_rgba_hosts_empty = false;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));

    rejected = eligible;
    rejected.active_asset_ledger_empty = false;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));

    rejected = eligible;
    rejected.staged_asset_ledger_empty = false;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));

    rejected = eligible;
    rejected.transaction_active = true;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));

    rejected = eligible;
    rejected.transaction_failed = true;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));

    rejected = eligible;
    rejected.destructive_runtime_failure = true;
    ERUI_TEST_CHECK(
        !can_begin_vector_only_epoch_during_rgba8_retirement(rejected));
}

void test_host_removal_rejects_invalid_counts_and_bounds() {
    ERUI_TEST_CHECK(plan_rgba8_host_removal(
        1U, 0U, 2U, 1U, {0U, 20U}).status ==
        Rgba8RetirementPolicyStatus::invalid_limits);
    ERUI_TEST_CHECK(plan_rgba8_host_removal(
        1U, 0U, 2U, 1U, {40U, 0U}).status ==
        Rgba8RetirementPolicyStatus::invalid_limits);
    ERUI_TEST_CHECK(plan_rgba8_host_removal(
        0U, 0U, 1U, 1U, kLimits).status ==
        Rgba8RetirementPolicyStatus::no_active_host);
    ERUI_TEST_CHECK(plan_rgba8_host_removal(
        41U, 0U, 42U, 41U, kLimits).status ==
        Rgba8RetirementPolicyStatus::active_hosts_out_of_range);
    ERUI_TEST_CHECK(plan_rgba8_host_removal(
        1U, 21U, 23U, 22U, kLimits).status ==
        Rgba8RetirementPolicyStatus::retirement_debt_out_of_range);
    ERUI_TEST_CHECK(plan_rgba8_host_removal(
        1U, 20U, 22U, 21U, kLimits).status ==
        Rgba8RetirementPolicyStatus::retirement_debt_capacity_exhausted);

    constexpr std::uint32_t maximum =
        std::numeric_limits<std::uint32_t>::max();
    ERUI_TEST_CHECK(plan_rgba8_host_removal(
        maximum, 0U, maximum, maximum, {maximum, maximum}).status ==
        Rgba8RetirementPolicyStatus::reference_count_overflow);
    ERUI_TEST_CHECK(plan_rgba8_host_removal(
        2U, 0U, 4U, 2U, kLimits).status ==
        Rgba8RetirementPolicyStatus::pre_removal_reference_mismatch);
    ERUI_TEST_CHECK(plan_rgba8_host_removal(
        2U, 0U, 3U, 1U, kLimits).status ==
        Rgba8RetirementPolicyStatus::observed_raw_below_minimum);
    ERUI_TEST_CHECK(plan_rgba8_host_removal(
        2U, 0U, 3U, 4U, kLimits).status ==
        Rgba8RetirementPolicyStatus::observed_raw_above_maximum);
}

void test_reconciliation_rejects_invalid_counts_and_bounds() {
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        0U, 0U, 1U, {0U, 20U}).status ==
        Rgba8RetirementPolicyStatus::invalid_limits);
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        41U, 0U, 42U, kLimits).status ==
        Rgba8RetirementPolicyStatus::active_hosts_out_of_range);
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        0U, 21U, 22U, kLimits).status ==
        Rgba8RetirementPolicyStatus::retirement_debt_out_of_range);

    constexpr std::uint32_t maximum =
        std::numeric_limits<std::uint32_t>::max();
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        maximum, 0U, maximum, {maximum, maximum}).status ==
        Rgba8RetirementPolicyStatus::reference_count_overflow);
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        0U, maximum, maximum, {maximum, maximum}).status ==
        Rgba8RetirementPolicyStatus::reference_count_overflow);
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        1U, 2U, 1U, kLimits).status ==
        Rgba8RetirementPolicyStatus::observed_raw_below_minimum);
    ERUI_TEST_CHECK(plan_rgba8_retirement_reconciliation(
        1U, 2U, 5U, kLimits).status ==
        Rgba8RetirementPolicyStatus::observed_raw_above_maximum);
}

} // namespace

int main() {
    test_synchronous_host_removal();
    test_two_delayed_host_removals_accumulate_debt();
    test_prior_debt_can_retire_during_a_later_host_removal();
    test_retirement_reconciliation_moves_only_downward();
    test_resource_identity_not_stable_tab_identity_controls_preservation();
    test_retired_and_current_resource_banks_reconcile_independently();
    test_live_overlap_uses_per_resource_host_counts();
    test_rapid_epoch_handoff_requires_complete_ownership_proof();
    test_host_removal_rejects_invalid_counts_and_bounds();
    test_reconciliation_rejects_invalid_counts_and_bounds();
    return 0;
}
