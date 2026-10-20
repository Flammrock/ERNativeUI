#include "tab_icon_retirement_policy.hpp"

#include "test_assertions.hpp"

#include <cstdint>
#include <limits>

namespace {

using erui::detail::Rgba8RetirementLimits;
using erui::detail::Rgba8RetirementPolicyStatus;
using erui::detail::plan_rgba8_host_removal;
using erui::detail::plan_rgba8_retirement_reconciliation;

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
    test_host_removal_rejects_invalid_counts_and_bounds();
    test_reconciliation_rejects_invalid_counts_and_bounds();
    return 0;
}
