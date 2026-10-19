#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace erui::detail {

inline constexpr std::size_t tab_record_epoch_slot_count = 8;
inline constexpr std::size_t tab_record_epoch_title_capacity = 96;
inline constexpr std::size_t tab_record_epoch_presentation_size = 0x40;

struct TabRecordObservation {
    std::uintptr_t source_title{};
    bool title_complete{};
    std::array<wchar_t, tab_record_epoch_title_capacity> title{};
    std::int32_t category{-1};
    std::array<std::byte, tab_record_epoch_presentation_size> presentation{};
};

struct StockTabRecordFingerprint {
    std::array<wchar_t, tab_record_epoch_title_capacity> title{};
    std::int32_t category{-1};
    std::array<std::byte, tab_record_epoch_presentation_size> presentation{};
};

enum class StockTabEpochMatch : std::uint8_t {
    exact,
    missing_title_pointer,
    borrowed_title_pointer,
    incomplete_title,
    category_mismatch,
    title_mismatch,
    presentation_mismatch,
};

struct StockTabEpochResult {
    StockTabEpochMatch match{StockTabEpochMatch::exact};
    std::size_t slot{tab_record_epoch_slot_count};

    [[nodiscard]] bool exact() const noexcept {
        return match == StockTabEpochMatch::exact;
    }
};

template <typename IsOwnedTitleAddress>
[[nodiscard]] inline StockTabEpochResult
classify_stock_tab_epoch_with_predicate(
    const std::array<
        TabRecordObservation,
        tab_record_epoch_slot_count>& observations,
    const std::array<
        StockTabRecordFingerprint,
        tab_record_epoch_slot_count>& stock,
    IsOwnedTitleAddress&& is_owned_title_address) noexcept {
    for (std::size_t slot = 0; slot < observations.size(); ++slot) {
        const TabRecordObservation& observed = observations[slot];
        const StockTabRecordFingerprint& expected = stock[slot];
        if (observed.source_title == 0) {
            return {StockTabEpochMatch::missing_title_pointer, slot};
        }
        if (is_owned_title_address(observed.source_title)) {
            return {StockTabEpochMatch::borrowed_title_pointer, slot};
        }
        if (!observed.title_complete) {
            return {StockTabEpochMatch::incomplete_title, slot};
        }
        if (observed.category != expected.category) {
            return {StockTabEpochMatch::category_mismatch, slot};
        }
        if (observed.title != expected.title) {
            return {StockTabEpochMatch::title_mismatch, slot};
        }
        if (observed.presentation != expected.presentation) {
            return {StockTabEpochMatch::presentation_mismatch, slot};
        }
    }
    return {};
}

[[nodiscard]] inline StockTabEpochResult classify_stock_tab_epoch(
    const std::array<
        TabRecordObservation,
        tab_record_epoch_slot_count>& observations,
    const std::array<
        StockTabRecordFingerprint,
        tab_record_epoch_slot_count>& stock,
    std::span<const std::uintptr_t> owned_title_addresses) noexcept {
    return classify_stock_tab_epoch_with_predicate(
        observations,
        stock,
        [owned_title_addresses](std::uintptr_t candidate) noexcept {
            for (const std::uintptr_t owned : owned_title_addresses) {
                if (owned != 0 && candidate == owned) return true;
            }
            return false;
        });
}

} // namespace erui::detail
