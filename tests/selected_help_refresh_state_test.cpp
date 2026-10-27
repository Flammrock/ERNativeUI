#include "native_row_presentation.hpp"

#include "test_assertions.hpp"

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kPhysicalIndex = 3;
constexpr std::size_t kPresentationSlot = 17;
constexpr std::uint64_t kPageEpoch = 41;
constexpr std::uint64_t kHelpRevision = 9;

int g_page{};
int g_row{};
wchar_t g_source[]{L"Updated help"};

erui::native::SelectedHelpRefreshTicket make_ticket() noexcept {
    return {
        .page = &g_page,
        .native_row = &g_row,
        .source = g_source,
        .physical_index = kPhysicalIndex,
        .presentation_slot = kPresentationSlot,
        .page_epoch = kPageEpoch,
        .help_revision = kHelpRevision,
        .armed = true,
    };
}

} // namespace

int main() {
    using erui::native::SelectedHelpRefreshDecision;
    using erui::native::SelectedHelpRefreshTicket;
    using erui::native::select_help_refresh_decision;

    const SelectedHelpRefreshTicket ticket = make_ticket();
    ERUI_TEST_CHECK(select_help_refresh_decision(
        ticket, false, kPageEpoch, 8, kPhysicalIndex) ==
        SelectedHelpRefreshDecision::publish);
    ERUI_TEST_CHECK(select_help_refresh_decision(
        ticket, true, kPageEpoch, 8, kPhysicalIndex) ==
        SelectedHelpRefreshDecision::defer);
    static_assert(select_help_refresh_decision(
        {}, false, kPageEpoch, 8, kPhysicalIndex) ==
        SelectedHelpRefreshDecision::none);

    ERUI_TEST_CHECK(select_help_refresh_decision(
        ticket, false, kPageEpoch + 1, 8, kPhysicalIndex) ==
        SelectedHelpRefreshDecision::discard);
    ERUI_TEST_CHECK(select_help_refresh_decision(
        ticket, false, kPageEpoch, 0, kPhysicalIndex) ==
        SelectedHelpRefreshDecision::discard);
    ERUI_TEST_CHECK(select_help_refresh_decision(
        ticket, false, kPageEpoch, 17, kPhysicalIndex) ==
        SelectedHelpRefreshDecision::discard);
    ERUI_TEST_CHECK(select_help_refresh_decision(
        ticket, false, kPageEpoch, 8, kPhysicalIndex + 1) ==
        SelectedHelpRefreshDecision::discard);

    SelectedHelpRefreshTicket invalid = ticket;
    invalid.source = nullptr;
    ERUI_TEST_CHECK(select_help_refresh_decision(
        invalid, false, kPageEpoch, 8, kPhysicalIndex) ==
        SelectedHelpRefreshDecision::discard);
    invalid = ticket;
    invalid.help_revision = 0;
    ERUI_TEST_CHECK(select_help_refresh_decision(
        invalid, false, kPageEpoch, 8, kPhysicalIndex) ==
        SelectedHelpRefreshDecision::discard);
    invalid = ticket;
    invalid.physical_index = erui::detail::invalid_compiled_index;
    ERUI_TEST_CHECK(select_help_refresh_decision(
        invalid, false, kPageEpoch, 8, kPhysicalIndex) ==
        SelectedHelpRefreshDecision::discard);

    return 0;
}
