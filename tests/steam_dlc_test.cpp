#include "steam_dlc.hpp"

#include "test_assertions.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

struct RetryFixture {
    std::array<erui::host::SteamDlcProbe, 20> results{};
    std::size_t result_count{};
    std::size_t next_result{};
    std::array<std::uint32_t, 20> waits{};
    std::size_t wait_count{};
};

erui::host::SteamDlcProbe next_probe(void* context) noexcept {
    auto& fixture = *static_cast<RetryFixture*>(context);
    const std::size_t index = fixture.next_result < fixture.result_count
        ? fixture.next_result
        : fixture.result_count - 1;
    ++fixture.next_result;
    return fixture.results[index];
}

void record_wait(void* context, std::uint32_t delay_ms) noexcept {
    auto& fixture = *static_cast<RetryFixture*>(context);
    fixture.waits[fixture.wait_count++] = delay_ms;
}

erui::host::SteamDlcProbe observed(
    erui::host::SteamDlcObservation observation) noexcept {
    return erui::host::evaluate_steam_dlc_observation(observation);
}

} // namespace

int main() {
    using erui::host::SteamDlcFailure;
    using erui::host::SteamDlcObservation;
    using erui::host::SteamDlcState;

    struct Case {
        SteamDlcObservation observation;
        SteamDlcState state;
        SteamDlcFailure failure;
    };
    constexpr std::array cases{
        Case{SteamDlcObservation::module_not_loaded,
            SteamDlcState::unknown, SteamDlcFailure::module_not_loaded},
        Case{SteamDlcObservation::exports_unavailable,
            SteamDlcState::unknown, SteamDlcFailure::exports_unavailable},
        Case{SteamDlcObservation::apps_not_initialized,
            SteamDlcState::unknown, SteamDlcFailure::apps_not_initialized},
        Case{SteamDlcObservation::absent,
            SteamDlcState::absent, SteamDlcFailure::none},
        Case{SteamDlcObservation::installed,
            SteamDlcState::installed, SteamDlcFailure::none},
        Case{SteamDlcObservation::exception,
            SteamDlcState::unknown, SteamDlcFailure::exception},
    };

    for (const Case& test : cases) {
        const auto result = erui::host::evaluate_steam_dlc_observation(
            test.observation);
        ERUI_TEST_CHECK(result.state == test.state);
        ERUI_TEST_CHECK(result.failure_code == test.failure);
        if (test.failure == SteamDlcFailure::none) {
            ERUI_TEST_CHECK(result.failure == nullptr);
        } else {
            ERUI_TEST_CHECK(result.failure != nullptr);
            ERUI_TEST_CHECK(result.failure[0] != '\0');
        }
    }

    ERUI_TEST_CHECK(erui::host::kShadowOfTheErdtreeAppId == 2778580u);
    ERUI_TEST_CHECK(erui::host::is_retryable_steam_dlc_failure(
        SteamDlcFailure::module_not_loaded));
    ERUI_TEST_CHECK(erui::host::is_retryable_steam_dlc_failure(
        SteamDlcFailure::apps_not_initialized));
    ERUI_TEST_CHECK(!erui::host::is_retryable_steam_dlc_failure(
        SteamDlcFailure::none));
    ERUI_TEST_CHECK(!erui::host::is_retryable_steam_dlc_failure(
        SteamDlcFailure::exports_unavailable));
    ERUI_TEST_CHECK(!erui::host::is_retryable_steam_dlc_failure(
        SteamDlcFailure::exception));

    {
        RetryFixture fixture{};
        fixture.results[0] = observed(SteamDlcObservation::installed);
        fixture.result_count = 1;
        const auto result = erui::host::retry_steam_dlc_probe(
            {.maximum_attempts = 8, .delay_between_attempts_ms = 10},
            &next_probe,
            &record_wait,
            &fixture);
        ERUI_TEST_CHECK(result.state == SteamDlcState::installed);
        ERUI_TEST_CHECK(result.attempts == 1);
        ERUI_TEST_CHECK(fixture.next_result == 1);
        ERUI_TEST_CHECK(fixture.wait_count == 0);
    }

    // Both startup races are retryable, and the first definitive answer ends
    // the sequence without an extra wait.
    {
        RetryFixture fixture{};
        fixture.results[0] = observed(
            SteamDlcObservation::module_not_loaded);
        fixture.results[1] = observed(
            SteamDlcObservation::apps_not_initialized);
        fixture.results[2] = observed(SteamDlcObservation::absent);
        fixture.result_count = 3;
        const auto result = erui::host::retry_steam_dlc_probe(
            {.maximum_attempts = 8, .delay_between_attempts_ms = 7},
            &next_probe,
            &record_wait,
            &fixture);
        ERUI_TEST_CHECK(result.state == SteamDlcState::absent);
        ERUI_TEST_CHECK(result.failure_code == SteamDlcFailure::none);
        ERUI_TEST_CHECK(result.attempts == 3);
        ERUI_TEST_CHECK(fixture.next_result == 3);
        ERUI_TEST_CHECK(fixture.wait_count == 2);
        ERUI_TEST_CHECK(fixture.waits[0] == 7);
        ERUI_TEST_CHECK(fixture.waits[1] == 7);
    }

    // A stable failure (missing exports or an exception) cannot improve by
    // waiting, so it must return immediately.
    for (const SteamDlcObservation terminal : {
             SteamDlcObservation::exports_unavailable,
             SteamDlcObservation::exception}) {
        RetryFixture fixture{};
        fixture.results[0] = observed(terminal);
        fixture.results[1] = observed(SteamDlcObservation::installed);
        fixture.result_count = 2;
        const auto result = erui::host::retry_steam_dlc_probe(
            {.maximum_attempts = 8, .delay_between_attempts_ms = 10},
            &next_probe,
            &record_wait,
            &fixture);
        ERUI_TEST_CHECK(result.state == SteamDlcState::unknown);
        ERUI_TEST_CHECK(result.attempts == 1);
        ERUI_TEST_CHECK(fixture.next_result == 1);
        ERUI_TEST_CHECK(fixture.wait_count == 0);
    }

    // Exhaustion keeps the final retryable result unknown. It never guesses
    // that the DLC is absent merely because Steam was not ready in time.
    {
        RetryFixture fixture{};
        fixture.results[0] = observed(
            SteamDlcObservation::apps_not_initialized);
        fixture.result_count = 1;
        const auto result = erui::host::retry_steam_dlc_probe(
            {.maximum_attempts = 4, .delay_between_attempts_ms = 3},
            &next_probe,
            &record_wait,
            &fixture);
        ERUI_TEST_CHECK(result.state == SteamDlcState::unknown);
        ERUI_TEST_CHECK(
            result.failure_code == SteamDlcFailure::apps_not_initialized);
        ERUI_TEST_CHECK(result.attempts == 4);
        ERUI_TEST_CHECK(fixture.next_result == 4);
        ERUI_TEST_CHECK(fixture.wait_count == 3);
    }

    // Zero attempts still performs the mandatory initial observation.
    {
        RetryFixture fixture{};
        fixture.results[0] = observed(
            SteamDlcObservation::module_not_loaded);
        fixture.result_count = 1;
        const auto result = erui::host::retry_steam_dlc_probe(
            {.maximum_attempts = 0, .delay_between_attempts_ms = 10},
            &next_probe,
            &record_wait,
            &fixture);
        ERUI_TEST_CHECK(result.state == SteamDlcState::unknown);
        ERUI_TEST_CHECK(result.attempts == 1);
        ERUI_TEST_CHECK(fixture.next_result == 1);
        ERUI_TEST_CHECK(fixture.wait_count == 0);
    }

    // Production safety caps both independently supplied policy values.
    {
        RetryFixture fixture{};
        fixture.results[0] = observed(
            SteamDlcObservation::module_not_loaded);
        fixture.result_count = 1;
        const auto result = erui::host::retry_steam_dlc_probe(
            {.maximum_attempts = 999, .delay_between_attempts_ms = 999},
            &next_probe,
            &record_wait,
            &fixture);
        ERUI_TEST_CHECK(
            result.attempts == erui::host::kSteamDlcRetryAttemptLimit);
        ERUI_TEST_CHECK(
            fixture.next_result == erui::host::kSteamDlcRetryAttemptLimit);
        ERUI_TEST_CHECK(
            fixture.wait_count + 1 == erui::host::kSteamDlcRetryAttemptLimit);
        for (std::size_t i = 0; i < fixture.wait_count; ++i) {
            ERUI_TEST_CHECK(
                fixture.waits[i] == erui::host::kSteamDlcRetryDelayLimitMs);
        }
    }

    {
        const auto result = erui::host::retry_steam_dlc_probe(
            erui::host::kDefaultSteamDlcRetryPolicy,
            nullptr,
            nullptr,
            nullptr);
        ERUI_TEST_CHECK(result.state == SteamDlcState::unknown);
        ERUI_TEST_CHECK(result.failure_code == SteamDlcFailure::exception);
        ERUI_TEST_CHECK(result.failure != nullptr);
        ERUI_TEST_CHECK(result.attempts == 1);
    }

    // The test runner may or may not have Steam loaded. Either observation is
    // valid, but the public result must always remain internally coherent.
    const auto live = erui::host::probe_shadow_of_the_erdtree_dlc();
    if (live.state == SteamDlcState::unknown) {
        ERUI_TEST_CHECK(live.failure_code != SteamDlcFailure::none);
        ERUI_TEST_CHECK(live.failure != nullptr);
    } else {
        ERUI_TEST_CHECK(live.failure_code == SteamDlcFailure::none);
        ERUI_TEST_CHECK(live.failure == nullptr);
    }

    ERUI_TEST_CHECK(
        erui::host::kDefaultSteamDlcRetryPolicy.maximum_attempts == 8);
    ERUI_TEST_CHECK(
        erui::host::kDefaultSteamDlcRetryPolicy
            .delay_between_attempts_ms == 10);

    return 0;
}
