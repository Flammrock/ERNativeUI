#include "tab_record_epoch.hpp"

#include "test_assertions.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using erui::detail::StockTabEpochMatch;
using erui::detail::StockTabRecordFingerprint;
using erui::detail::TabRecordObservation;
using erui::detail::classify_stock_tab_epoch;
using erui::detail::tab_record_epoch_slot_count;

template <std::size_t Size>
void set_title(
    std::array<wchar_t, Size>& destination,
    const wchar_t* source) {
    destination.fill(L'\0');
    std::size_t index{};
    while (source[index] != L'\0' && index + 1 < destination.size()) {
        destination[index] = source[index];
        ++index;
    }
}

struct Fixture {
    std::array<TabRecordObservation, tab_record_epoch_slot_count>
        observations{};
    std::array<StockTabRecordFingerprint, tab_record_epoch_slot_count>
        stock{};
    std::array<std::uintptr_t, 12> owned_titles{};
};

Fixture make_fixture() {
    constexpr std::array<const wchar_t*, tab_record_epoch_slot_count> titles{
        L"Game Options",
        L"Camera Options",
        L"Display",
        L"Sound",
        L"Network",
        L"Controller Settings",
        L"Keyboard/Mouse Settings",
        L"Graphics",
    };
    constexpr std::array<std::int32_t, tab_record_epoch_slot_count>
        categories{0, 1, 2, 3, 5, 6, 7, 8};

    Fixture fixture{};
    for (std::size_t slot = 0; slot < fixture.stock.size(); ++slot) {
        set_title(fixture.stock[slot].title, titles[slot]);
        fixture.stock[slot].category = categories[slot];
        for (std::size_t byte = 0;
             byte < fixture.stock[slot].presentation.size(); ++byte) {
            fixture.stock[slot].presentation[byte] = std::byte{
                static_cast<unsigned char>((slot * 17 + byte) & 0xFF)};
        }

        fixture.observations[slot].source_title =
            0x1000 + slot * 0x100;
        fixture.observations[slot].title_complete = true;
        fixture.observations[slot].title = fixture.stock[slot].title;
        fixture.observations[slot].category = fixture.stock[slot].category;
        fixture.observations[slot].presentation =
            fixture.stock[slot].presentation;
    }
    for (std::size_t index = 0; index < fixture.owned_titles.size(); ++index) {
        fixture.owned_titles[index] = 0x9000 + index * 0x100;
    }
    return fixture;
}

void expect_mismatch(
    const Fixture& fixture,
    StockTabEpochMatch expected,
    std::size_t expected_slot) {
    const auto result = classify_stock_tab_epoch(
        fixture.observations,
        fixture.stock,
        fixture.owned_titles);
    ERUI_TEST_CHECK(!result.exact());
    ERUI_TEST_CHECK(result.match == expected);
    ERUI_TEST_CHECK(result.slot == expected_slot);
}

void test_exact_external_stock_layout() {
    const Fixture fixture = make_fixture();
    const auto result = classify_stock_tab_epoch(
        fixture.observations,
        fixture.stock,
        fixture.owned_titles);
    ERUI_TEST_CHECK(result.exact());
    ERUI_TEST_CHECK(result.slot == tab_record_epoch_slot_count);
}

void test_title_pointer_guards() {
    Fixture fixture = make_fixture();
    fixture.observations[2].source_title = 0;
    expect_mismatch(
        fixture, StockTabEpochMatch::missing_title_pointer, 2);

    fixture = make_fixture();
    fixture.observations[3].source_title = fixture.owned_titles[3];
    expect_mismatch(
        fixture, StockTabEpochMatch::borrowed_title_pointer, 3);

    fixture = make_fixture();
    // A stock-looking slot borrowing any other provider-owned/custom title
    // is still the current viewport lifetime, not a native stock republish.
    fixture.observations[1].source_title = fixture.owned_titles[10];
    expect_mismatch(
        fixture, StockTabEpochMatch::borrowed_title_pointer, 1);

    fixture = make_fixture();
    fixture.observations[6].title_complete = false;
    expect_mismatch(fixture, StockTabEpochMatch::incomplete_title, 6);
}

void test_stock_fingerprint_guards() {
    Fixture fixture = make_fixture();
    fixture.observations[4].category = 4;
    expect_mismatch(fixture, StockTabEpochMatch::category_mismatch, 4);

    fixture = make_fixture();
    fixture.observations[5].title[0] = L'X';
    expect_mismatch(fixture, StockTabEpochMatch::title_mismatch, 5);

    fixture = make_fixture();
    fixture.observations[7].presentation[19] ^= std::byte{0x01};
    expect_mismatch(
        fixture, StockTabEpochMatch::presentation_mismatch, 7);
}

} // namespace

int main() {
    test_exact_external_stock_layout();
    test_title_pointer_guards();
    test_stock_fingerprint_guards();
    return 0;
}
