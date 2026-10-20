#pragma once

#include <cstdint>
#include <limits>

namespace erui::detail {

// Every retained RawImage owns one process-lifetime root reference. A live
// Shape contributes one additional reference, while a detached Shape may
// leave that reference behind until Scaleform reaches a renderer/trace
// boundary. Retirement debt records only those detached references.
inline constexpr std::uint32_t rgba8_root_reference_count = 1U;

struct Rgba8RetirementLimits final {
    std::uint32_t maximum_live_hosts{};
    std::uint32_t maximum_retirement_debt{};
};

enum class Rgba8RetirementPolicyStatus : std::uint8_t {
    accepted,
    invalid_limits,
    no_active_host,
    active_hosts_out_of_range,
    retirement_debt_out_of_range,
    retirement_debt_capacity_exhausted,
    reference_count_overflow,
    pre_removal_reference_mismatch,
    observed_raw_below_minimum,
    observed_raw_above_maximum,
};

struct Rgba8HostRemovalPlan final {
    Rgba8RetirementPolicyStatus status{
        Rgba8RetirementPolicyStatus::invalid_limits};
    std::uint32_t active_hosts_after{};
    std::uint32_t retirement_debt_after{};
    std::uint32_t minimum_raw_after{};
    std::uint32_t maximum_raw_after{};

    [[nodiscard]] constexpr bool accepted() const noexcept {
        return status == Rgba8RetirementPolicyStatus::accepted;
    }

    [[nodiscard]] constexpr bool retirement_pending() const noexcept {
        return accepted() && retirement_debt_after != 0U;
    }
};

struct Rgba8RetirementReconciliationPlan final {
    Rgba8RetirementPolicyStatus status{
        Rgba8RetirementPolicyStatus::invalid_limits};
    std::uint32_t retirement_debt_after{};
    std::uint32_t minimum_raw{};
    std::uint32_t maximum_raw{};

    [[nodiscard]] constexpr bool accepted() const noexcept {
        return status == Rgba8RetirementPolicyStatus::accepted;
    }

    [[nodiscard]] constexpr bool complete() const noexcept {
        return accepted() && retirement_debt_after == 0U;
    }
};

[[nodiscard]] constexpr bool rgba8_retirement_limits_valid(
    Rgba8RetirementLimits limits) noexcept {
    return limits.maximum_live_hosts != 0U &&
        limits.maximum_retirement_debt != 0U;
}

[[nodiscard]] constexpr bool rgba8_reference_sum_overflows(
    std::uint32_t active_hosts,
    std::uint32_t retirement_debt) noexcept {
    constexpr std::uint32_t maximum =
        std::numeric_limits<std::uint32_t>::max();
    if (retirement_debt > maximum - rgba8_root_reference_count) {
        return true;
    }
    return active_hosts >
        maximum - rgba8_root_reference_count - retirement_debt;
}

// Plan the bookkeeping transition after one exact same-Movie Shape removal.
// The caller remains responsible for proving the native operation itself:
// Graphics.clear(), removeChild(), property cleanup, wrapper destruction, and
// Resource refcount 1. This policy validates only the RawImage ownership
// arithmetic and deliberately reserves room for either an immediate release
// or one newly delayed renderer reference before mutation begins.
[[nodiscard]] constexpr Rgba8HostRemovalPlan plan_rgba8_host_removal(
    std::uint32_t active_hosts_before,
    std::uint32_t retirement_debt_before,
    std::uint32_t raw_references_before,
    std::uint32_t raw_references_after,
    Rgba8RetirementLimits limits) noexcept {
    Rgba8HostRemovalPlan result{};
    if (!rgba8_retirement_limits_valid(limits)) {
        return result;
    }
    if (active_hosts_before == 0U) {
        result.status = Rgba8RetirementPolicyStatus::no_active_host;
        return result;
    }
    if (active_hosts_before > limits.maximum_live_hosts) {
        result.status =
            Rgba8RetirementPolicyStatus::active_hosts_out_of_range;
        return result;
    }
    if (retirement_debt_before >
        limits.maximum_retirement_debt) {
        result.status =
            Rgba8RetirementPolicyStatus::retirement_debt_out_of_range;
        return result;
    }
    if (retirement_debt_before ==
        limits.maximum_retirement_debt) {
        result.status = Rgba8RetirementPolicyStatus::
            retirement_debt_capacity_exhausted;
        return result;
    }
    if (rgba8_reference_sum_overflows(
            active_hosts_before,
            retirement_debt_before)) {
        result.status =
            Rgba8RetirementPolicyStatus::reference_count_overflow;
        return result;
    }

    const std::uint32_t expected_raw_before =
        rgba8_root_reference_count + active_hosts_before +
        retirement_debt_before;
    if (raw_references_before != expected_raw_before) {
        result.status = Rgba8RetirementPolicyStatus::
            pre_removal_reference_mismatch;
        return result;
    }

    result.active_hosts_after = active_hosts_before - 1U;
    result.minimum_raw_after =
        rgba8_root_reference_count + result.active_hosts_after;
    // The upper bound is the pre-removal reference count: the detached Shape
    // may retain its one reference, but an exact removal cannot create more.
    result.maximum_raw_after = expected_raw_before;
    if (raw_references_after < result.minimum_raw_after) {
        result.status =
            Rgba8RetirementPolicyStatus::observed_raw_below_minimum;
        return result;
    }
    if (raw_references_after > result.maximum_raw_after) {
        result.status =
            Rgba8RetirementPolicyStatus::observed_raw_above_maximum;
        return result;
    }

    result.retirement_debt_after =
        raw_references_after - result.minimum_raw_after;
    result.status = Rgba8RetirementPolicyStatus::accepted;
    return result;
}

// Reconcile previously recorded debt without changing live host ownership.
// The observed count may remain unchanged or move downward; an increase, a
// drop below root+live-host ownership, or arithmetic outside configured
// bounds is rejected.
[[nodiscard]] constexpr Rgba8RetirementReconciliationPlan
plan_rgba8_retirement_reconciliation(
    std::uint32_t active_hosts,
    std::uint32_t retirement_debt_before,
    std::uint32_t raw_references,
    Rgba8RetirementLimits limits) noexcept {
    Rgba8RetirementReconciliationPlan result{};
    if (!rgba8_retirement_limits_valid(limits)) {
        return result;
    }
    if (active_hosts > limits.maximum_live_hosts) {
        result.status =
            Rgba8RetirementPolicyStatus::active_hosts_out_of_range;
        return result;
    }
    if (retirement_debt_before >
        limits.maximum_retirement_debt) {
        result.status =
            Rgba8RetirementPolicyStatus::retirement_debt_out_of_range;
        return result;
    }
    if (rgba8_reference_sum_overflows(
            active_hosts,
            retirement_debt_before)) {
        result.status =
            Rgba8RetirementPolicyStatus::reference_count_overflow;
        return result;
    }

    result.minimum_raw = rgba8_root_reference_count + active_hosts;
    result.maximum_raw = result.minimum_raw + retirement_debt_before;
    if (raw_references < result.minimum_raw) {
        result.status =
            Rgba8RetirementPolicyStatus::observed_raw_below_minimum;
        return result;
    }
    if (raw_references > result.maximum_raw) {
        result.status =
            Rgba8RetirementPolicyStatus::observed_raw_above_maximum;
        return result;
    }

    result.retirement_debt_after = raw_references - result.minimum_raw;
    result.status = Rgba8RetirementPolicyStatus::accepted;
    return result;
}

} // namespace erui::detail
