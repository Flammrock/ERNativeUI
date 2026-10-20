#include "gfx/title_gfx_runtime.hpp"

#include "test_assertions.hpp"

#include <algorithm>
#include <array>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using erui::gfx::GfxDocument;
using erui::gfx::TitleGfxCorner;
using erui::gfx::TitleGfxDlcState;
using erui::gfx::TitleGfxRgb;
using erui::gfx::TitleGfxRuntimeCache;
using erui::gfx::TitleGfxRuntimeErrorCode;
using erui::gfx::TitleGfxRuntimeLine;
using erui::gfx::TitleGfxRuntimeOutcome;
using erui::gfx::TitleGfxRuntimePlanState;
using erui::gfx::TitleGfxRuntimePublishStatus;
using erui::gfx::TitleGfxRuntimeResult;
using erui::gfx::TitleGfxRuntimeSourceProfile;
using erui::gfx::TitleGfxTextOccupancy;
using erui::gfx::TitleGfxTextSide;

constexpr std::uint16_t kTagEnd = 0;
constexpr std::uint16_t kTagShowFrame = 1;
constexpr std::uint16_t kTagPlaceObject2 = 26;
constexpr std::uint16_t kTagDefineEditText = 37;
constexpr std::uint16_t kTagDefineSprite = 39;
constexpr std::uint16_t kUnknownTag = 777;

class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& output) noexcept
        : output_(output) {}

    void write(std::uint32_t value, std::size_t count) {
        for (std::size_t remaining = count; remaining != 0U; --remaining) {
            if (bit_offset_ == 0U) output_.push_back(0);
            output_.back() |= static_cast<std::uint8_t>(
                ((value >> (remaining - 1U)) & 1U) <<
                (7U - bit_offset_));
            bit_offset_ = (bit_offset_ + 1U) & 7U;
        }
    }

    void write_signed(std::int32_t value, std::size_t count) {
        const std::uint32_t mask = count == 32U
            ? std::numeric_limits<std::uint32_t>::max()
            : ((std::uint32_t{1} << count) - 1U);
        write(static_cast<std::uint32_t>(value) & mask, count);
    }

    void align() noexcept { bit_offset_ = 0; }

private:
    std::vector<std::uint8_t>& output_;
    std::size_t bit_offset_{};
};

[[nodiscard]] std::size_t signed_bits(std::int32_t value) noexcept {
    for (std::size_t count = 1U; count < 32U; ++count) {
        const auto minimum = -(std::int64_t{1} << (count - 1U));
        const auto maximum = (std::int64_t{1} << (count - 1U)) - 1;
        if (value >= minimum && value <= maximum) return count;
    }
    return 32U;
}

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value >> 16U));
    output.push_back(static_cast<std::uint8_t>(value >> 24U));
}

void write_u32(
    std::vector<std::uint8_t>& output,
    std::size_t offset,
    std::uint32_t value) {
    output[offset] = static_cast<std::uint8_t>(value);
    output[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
    output[offset + 2U] = static_cast<std::uint8_t>(value >> 16U);
    output[offset + 3U] = static_cast<std::uint8_t>(value >> 24U);
}

void append_rect(
    std::vector<std::uint8_t>& output,
    std::int32_t x_min,
    std::int32_t x_max,
    std::int32_t y_min,
    std::int32_t y_max) {
    const std::size_t count = std::max({
        signed_bits(x_min), signed_bits(x_max),
        signed_bits(y_min), signed_bits(y_max)});
    BitWriter writer(output);
    writer.write(static_cast<std::uint32_t>(count), 5U);
    writer.write_signed(x_min, count);
    writer.write_signed(x_max, count);
    writer.write_signed(y_min, count);
    writer.write_signed(y_max, count);
    writer.align();
}

void append_matrix(
    std::vector<std::uint8_t>& output,
    std::int32_t x,
    std::int32_t y) {
    const std::size_t count = std::max(signed_bits(x), signed_bits(y));
    BitWriter writer(output);
    writer.write(0U, 1U);
    writer.write(0U, 1U);
    writer.write(static_cast<std::uint32_t>(count), 5U);
    writer.write_signed(x, count);
    writer.write_signed(y, count);
    writer.align();
}

void append_tag(
    std::vector<std::uint8_t>& output,
    std::uint16_t code,
    std::span<const std::uint8_t> body = {}) {
    if (body.size() < 0x3FU) {
        append_u16(output, static_cast<std::uint16_t>(
            (code << 6U) | body.size()));
    } else {
        append_u16(output, static_cast<std::uint16_t>(
            (code << 6U) | 0x3FU));
        append_u32(output, static_cast<std::uint32_t>(body.size()));
    }
    output.insert(output.end(), body.begin(), body.end());
}

[[nodiscard]] std::vector<std::uint8_t> edit_text(
    std::uint16_t character_id,
    std::string_view initial_text = "ONLINEEE") {
    std::vector<std::uint8_t> body{};
    append_u16(body, character_id);
    append_rect(body, -40, 4670, -40, 520);
    body.push_back(0xECU);
    body.push_back(0xA1U);
    constexpr std::string_view font_class = "MenuFont_01";
    body.insert(body.end(), font_class.begin(), font_class.end());
    body.push_back(0);
    append_u16(body, 320U);
    body.insert(body.end(), {0xFFU, 0xFFU, 0xFFU, 0xFFU});
    body.push_back(1U);
    body.insert(body.end(), 8U, 0U);
    body.push_back(0);
    body.insert(body.end(), initial_text.begin(), initial_text.end());
    body.push_back(0);
    return body;
}

[[nodiscard]] std::vector<std::uint8_t> named_placement(
    std::uint16_t character_id,
    std::uint16_t depth,
    std::int32_t x,
    std::int32_t y,
    std::string_view name) {
    std::vector<std::uint8_t> body{};
    body.push_back(0x26U);
    append_u16(body, depth);
    append_u16(body, character_id);
    append_matrix(body, x, y);
    body.insert(body.end(), name.begin(), name.end());
    body.push_back(0);
    return body;
}

[[nodiscard]] std::vector<std::uint8_t> sprite(
    std::uint16_t character_id,
    std::span<const std::vector<std::uint8_t>> placement_bodies) {
    std::vector<std::uint8_t> body{};
    append_u16(body, character_id);
    append_u16(body, 1U);
    for (const auto& placement : placement_bodies) {
        append_tag(body, kTagPlaceObject2, placement);
    }
    append_tag(body, kTagShowFrame);
    append_tag(body, kTagEnd);
    return body;
}

[[nodiscard]] std::vector<std::uint8_t> title_fixture(
    bool compatible_info_root = true) {
    constexpr std::array<std::string_view, 4> names{
        "RegulationVersion", "AppVersion", "OnlineMode", "DLCVersion_0"};
    constexpr std::array<std::int32_t, 4> y{1236, 757, 278, -680};

    std::vector<std::uint8_t> movie{
        static_cast<std::uint8_t>('G'),
        static_cast<std::uint8_t>('F'),
        static_cast<std::uint8_t>('X'),
        10U,
        0U, 0U, 0U, 0U};
    append_rect(movie, 0, 38400, 0, 21600);
    append_u16(movie, 0x1E00U);
    append_u16(movie, 1U);

    for (std::size_t index = 0; index < names.size(); ++index) {
        const auto definition = edit_text(
            static_cast<std::uint16_t>(100U + index));
        append_tag(movie, kTagDefineEditText, definition);
    }

    std::vector<std::vector<std::uint8_t>> info_children{};
    for (std::size_t index = 0; index < names.size(); ++index) {
        info_children.push_back(named_placement(
            static_cast<std::uint16_t>(100U + index),
            static_cast<std::uint16_t>(index + 1U),
            -4640,
            y[index],
            names[index]));
    }
    const auto info = sprite(300U, info_children);
    append_tag(movie, kTagDefineSprite, info);

    const std::array root_child{named_placement(
        300U,
        20U,
        12582,
        8138,
        compatible_info_root ? "Info" : "ForeignInfo")};
    const auto root = sprite(301U, root_child);
    append_tag(movie, kTagDefineSprite, root);

    const auto top = named_placement(
        301U, 1U, 19200, 10800, "Top");
    append_tag(movie, kTagPlaceObject2, top);

    const std::array<std::uint8_t, 2> unknown{0xA5U, 0x5AU};
    append_tag(movie, kUnknownTag, unknown);
    append_tag(movie, kTagShowFrame);
    append_tag(movie, kTagEnd);
    write_u32(movie, 4U, static_cast<std::uint32_t>(movie.size()));
    return movie;
}

[[nodiscard]] std::vector<std::uint8_t> padded_title_fixture(
    std::size_t target_size) {
    std::vector<std::uint8_t> movie = title_fixture();
    ERUI_TEST_CHECK(target_size > movie.size() + 6U);
    const std::size_t body_size = target_size - movie.size() - 6U;
    std::vector<std::uint8_t> body(body_size, 0x5AU);
    std::vector<std::uint8_t> encoded{};
    append_tag(encoded, kUnknownTag, body);
    ERUI_TEST_CHECK(encoded.size() == body_size + 6U);
    movie.insert(movie.end() - 4, encoded.begin(), encoded.end());
    ERUI_TEST_CHECK(movie.size() == target_size);
    write_u32(movie, 4U, static_cast<std::uint32_t>(movie.size()));
    return movie;
}

[[nodiscard]] std::vector<TitleGfxRuntimeLine> two_line_plan() {
    std::vector<TitleGfxRuntimeLine> lines{};
    lines.push_back({
        "provider.a", "First line", std::nullopt,
        TitleGfxCorner::bottom_right});
    lines.push_back({
        "provider.b",
        "UTF-8: Caf\xC3\xA9",
        TitleGfxRgb{216U, 167U, 103U},
        TitleGfxCorner::top_left});
    return lines;
}

void test_exact_path_classifier_and_names() {
    ERUI_TEST_CHECK(erui::gfx::is_title_gfx_runtime_path(
        "data0:/menu/05_000_title.gfx"));
    ERUI_TEST_CHECK(!erui::gfx::is_title_gfx_runtime_path(
        "data0:/menu/05_001_title_logo.gfx"));
    ERUI_TEST_CHECK(!erui::gfx::is_title_gfx_runtime_path(
        "data0:/menu/win/05_000_title.gfx"));
    ERUI_TEST_CHECK(!erui::gfx::is_title_gfx_runtime_path(
        "DATA0:/menu/05_000_title.gfx"));
    ERUI_TEST_CHECK(std::string_view(
        erui::gfx::title_gfx_runtime_plan_state_name(
            TitleGfxRuntimePlanState::ready_lines)) == "ready_lines");
    ERUI_TEST_CHECK(std::string_view(
        erui::gfx::title_gfx_runtime_outcome_name(
            TitleGfxRuntimeOutcome::transformed)) == "transformed");
    ERUI_TEST_CHECK(std::string_view(
        erui::gfx::title_gfx_runtime_error_name(
            TitleGfxRuntimeErrorCode::source_changed)) == "source_changed");
    ERUI_TEST_CHECK(std::string_view(
        erui::gfx::title_gfx_runtime_source_profile_name(
            TitleGfxRuntimeSourceProfile::unrecognized)) == "unrecognized");
    ERUI_TEST_CHECK(erui::gfx::classify_title_gfx_runtime_source(
        title_fixture()) == TitleGfxRuntimeSourceProfile::unrecognized);
}

void test_unrecognized_source_cannot_reuse_the_native_dlc_row() {
    const auto source = title_fixture();
    TitleGfxRuntimeCache cache{};
    std::vector<TitleGfxRuntimeLine> lines{
        {
            "provider.a", "First", std::nullopt,
            TitleGfxCorner::bottom_right,
        },
        {
            "provider.a", "Second", std::nullopt,
            TitleGfxCorner::bottom_right,
        },
    };
    ERUI_TEST_CHECK(cache.publish(
        std::move(lines), TitleGfxDlcState::absent) ==
        TitleGfxRuntimePublishStatus::published);

    const auto transformed = cache.prepare(source);
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(transformed.has_replacement());
    ERUI_TEST_CHECK(transformed.source_profile ==
        TitleGfxRuntimeSourceProfile::unrecognized);
    ERUI_TEST_CHECK(!transformed.dlc_row_reuse_permitted);
    ERUI_TEST_CHECK(transformed.after.first_injected_y == -1638);

    const auto cached = cache.prepare(source);
    ERUI_TEST_CHECK(cached.cache_hit);
    ERUI_TEST_CHECK(cached.source_profile ==
        TitleGfxRuntimeSourceProfile::unrecognized);
    ERUI_TEST_CHECK(!cached.dlc_row_reuse_permitted);
}

void test_external_occupancy_is_owned_and_reaches_the_transformer() {
    const auto source = title_fixture();
    TitleGfxRuntimeCache cache{};
    auto lines = two_line_plan();
    std::vector<TitleGfxTextOccupancy> occupancy{
        {TitleGfxTextSide::right, -1159}};
    ERUI_TEST_CHECK(cache.publish(
        std::move(lines),
        TitleGfxDlcState::unknown,
        std::move(occupancy)) == TitleGfxRuntimePublishStatus::published);
    occupancy.clear();

    const auto summary = cache.plan_summary();
    ERUI_TEST_CHECK(summary.external_occupancy_count == 1U);
    const auto transformed = cache.prepare(source);
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(transformed.has_replacement());
    ERUI_TEST_CHECK(transformed.after.first_injected_y == -1638);

    TitleGfxRuntimeCache oversized{};
    std::vector<TitleGfxTextOccupancy> excessive(
        erui::gfx::kTitleGfxRuntimeMaximumExternalOccupancy + 1U,
        {TitleGfxTextSide::right, 0});
    ERUI_TEST_CHECK(oversized.publish(
        two_line_plan(),
        TitleGfxDlcState::unknown,
        std::move(excessive)) ==
        TitleGfxRuntimePublishStatus::too_many_lines);
    ERUI_TEST_CHECK(oversized.plan_state() ==
        TitleGfxRuntimePlanState::pending);
}

void test_real_title_profile(const char* path) {
    std::ifstream input(path, std::ios::binary);
    ERUI_TEST_CHECK(input.good());
    const std::vector<std::uint8_t> source(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{});
    ERUI_TEST_CHECK(erui::gfx::classify_title_gfx_runtime_source(source) ==
        TitleGfxRuntimeSourceProfile::elden_ring_2_7_1_app_1_17_1);

    TitleGfxRuntimeCache cache{};
    std::vector<TitleGfxRuntimeLine> lines{
        {
            "provider.a", "First", std::nullopt,
            TitleGfxCorner::bottom_right,
        },
        {
            "provider.a", "Second", std::nullopt,
            TitleGfxCorner::bottom_right,
        },
    };
    ERUI_TEST_CHECK(cache.publish(
        std::move(lines), TitleGfxDlcState::absent) ==
        TitleGfxRuntimePublishStatus::published);
    const auto transformed = cache.prepare(source);
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(transformed.has_replacement());
    ERUI_TEST_CHECK(transformed.source_profile ==
        TitleGfxRuntimeSourceProfile::elden_ring_2_7_1_app_1_17_1);
    ERUI_TEST_CHECK(transformed.dlc_row_reuse_permitted);
    ERUI_TEST_CHECK(transformed.after.first_injected_y == -680);

    TitleGfxRuntimeCache occupied{};
    std::vector<TitleGfxRuntimeLine> occupied_lines{
        {
            "provider.a", "First", std::nullopt,
            TitleGfxCorner::bottom_right,
        },
        {
            "provider.a", "Second", std::nullopt,
            TitleGfxCorner::bottom_right,
        },
    };
    ERUI_TEST_CHECK(occupied.publish(
        std::move(occupied_lines),
        TitleGfxDlcState::absent,
        {{TitleGfxTextSide::right, 0}}) ==
        TitleGfxRuntimePublishStatus::published);
    const auto shifted = occupied.prepare(source);
    ERUI_TEST_CHECK(shifted.success());
    ERUI_TEST_CHECK(shifted.has_replacement());
    ERUI_TEST_CHECK(shifted.dlc_row_reuse_permitted);
    ERUI_TEST_CHECK(shifted.after.first_injected_y == -1159);
}

void test_pending_and_empty_publication_are_non_mutating() {
    const auto source = title_fixture();
    TitleGfxRuntimeCache cache{};
    ERUI_TEST_CHECK(cache.ready());
    ERUI_TEST_CHECK(cache.plan_state() == TitleGfxRuntimePlanState::pending);
    ERUI_TEST_CHECK(cache.provider_id_for_line(0U).empty());
    const auto pending_summary = cache.plan_summary();
    ERUI_TEST_CHECK(pending_summary.line_count == 0U);
    ERUI_TEST_CHECK(pending_summary.provider_count == 0U);

    const auto pending = cache.prepare(source);
    ERUI_TEST_CHECK(pending.success());
    ERUI_TEST_CHECK(pending.outcome == TitleGfxRuntimeOutcome::pending);
    ERUI_TEST_CHECK(!pending.has_replacement());

    ERUI_TEST_CHECK(cache.publish({}, TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::published);
    ERUI_TEST_CHECK(cache.plan_state() ==
        TitleGfxRuntimePlanState::ready_empty);
    const auto empty = cache.prepare(source);
    ERUI_TEST_CHECK(empty.success());
    ERUI_TEST_CHECK(empty.outcome == TitleGfxRuntimeOutcome::empty);
    ERUI_TEST_CHECK(!empty.cache_hit);
    ERUI_TEST_CHECK(!empty.has_replacement());

    ERUI_TEST_CHECK(cache.publish(
        two_line_plan(), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::already_published);
    ERUI_TEST_CHECK(cache.plan_state() ==
        TitleGfxRuntimePlanState::ready_empty);
}

void test_oversized_rejection_does_not_settle_publication() {
    TitleGfxRuntimeCache cache{};
    std::vector<TitleGfxRuntimeLine> oversized(
        erui::gfx::kTitleGfxRuntimeMaximumLines + 1U);
    for (auto& line : oversized) {
        line.provider_id = "provider";
        line.text = "line";
    }
    ERUI_TEST_CHECK(cache.publish(
        std::move(oversized), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::too_many_lines);
    ERUI_TEST_CHECK(cache.plan_state() == TitleGfxRuntimePlanState::pending);
    ERUI_TEST_CHECK(cache.publish({}, TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::published);
}

void test_pending_prepare_does_not_capture_source() {
    const auto source = title_fixture();
    TitleGfxRuntimeCache cache{};
    const auto pending = cache.prepare(source);
    ERUI_TEST_CHECK(pending.outcome == TitleGfxRuntimeOutcome::pending);
    ERUI_TEST_CHECK(!pending.cache_hit);

    ERUI_TEST_CHECK(cache.publish(
        two_line_plan(), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::published);
    const auto transformed = cache.prepare(source);
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(
        transformed.outcome == TitleGfxRuntimeOutcome::transformed);
    ERUI_TEST_CHECK(!transformed.cache_hit);
    ERUI_TEST_CHECK(transformed.has_replacement());
}

void test_bounded_plan_wait_times_out_without_settling_publication() {
    TitleGfxRuntimeCache cache{};
    const auto started = std::chrono::steady_clock::now();
    const auto state = cache.wait_for_plan(20U);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    ERUI_TEST_CHECK(state == TitleGfxRuntimePlanState::pending);
    ERUI_TEST_CHECK(elapsed >= std::chrono::milliseconds{5});
    // A platform scheduling delay may exceed the requested timeout, but a
    // lost wake or accidental unbounded wait must not strand this test.
    ERUI_TEST_CHECK(elapsed < std::chrono::seconds{2});
    ERUI_TEST_CHECK(cache.plan_state() == TitleGfxRuntimePlanState::pending);

    ERUI_TEST_CHECK(cache.publish(
        two_line_plan(), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::published);
    ERUI_TEST_CHECK(cache.wait_for_plan(2000U) ==
        TitleGfxRuntimePlanState::ready_lines);
}

void test_waiting_first_open_observes_plan_and_transforms() {
    constexpr std::size_t waiter_count = 12U;
    const auto source = title_fixture();
    TitleGfxRuntimeCache cache{};
    std::barrier start{static_cast<std::ptrdiff_t>(waiter_count + 1U)};
    std::array<TitleGfxRuntimePlanState, waiter_count> states{};
    std::array<TitleGfxRuntimeResult, waiter_count> results{};
    std::vector<std::thread> waiters{};
    waiters.reserve(waiter_count);
    for (std::size_t index = 0; index < waiter_count; ++index) {
        waiters.emplace_back([&, index] {
            start.arrive_and_wait();
            states[index] = cache.wait_for_plan(5000U);
            if (states[index] == TitleGfxRuntimePlanState::ready_lines) {
                results[index] = cache.prepare(source);
            }
        });
    }

    start.arrive_and_wait();
    ERUI_TEST_CHECK(cache.publish(
        two_line_plan(), TitleGfxDlcState::absent) ==
        TitleGfxRuntimePublishStatus::published);
    for (auto& waiter : waiters) waiter.join();

    const std::uint8_t* replacement{};
    std::size_t replacement_size{};
    std::size_t initial_preparations{};
    for (std::size_t index = 0; index < waiter_count; ++index) {
        ERUI_TEST_CHECK(states[index] ==
            TitleGfxRuntimePlanState::ready_lines);
        ERUI_TEST_CHECK(results[index].success());
        ERUI_TEST_CHECK(results[index].has_replacement());
        if (!results[index].cache_hit) ++initial_preparations;
        if (!replacement) {
            replacement = results[index].replacement_bytes;
            replacement_size = results[index].replacement_size;
        } else {
            ERUI_TEST_CHECK(results[index].replacement_bytes == replacement);
            ERUI_TEST_CHECK(
                results[index].replacement_size == replacement_size);
        }
    }
    ERUI_TEST_CHECK(initial_preparations == 1U);
}

void test_plan_summary_and_publish_once() {
    TitleGfxRuntimeCache cache{};
    auto lines = two_line_plan();
    lines.push_back({
        "provider.a", "Third line", std::nullopt,
        TitleGfxCorner::bottom_left});
    ERUI_TEST_CHECK(cache.publish(
        std::move(lines), TitleGfxDlcState::installed) ==
        TitleGfxRuntimePublishStatus::published);
    const auto summary = cache.plan_summary();
    ERUI_TEST_CHECK(summary.state == TitleGfxRuntimePlanState::ready_lines);
    ERUI_TEST_CHECK(summary.dlc_state == TitleGfxDlcState::installed);
    ERUI_TEST_CHECK(summary.line_count == 3U);
    ERUI_TEST_CHECK(summary.provider_count == 2U);
    ERUI_TEST_CHECK(summary.custom_color_count == 1U);
    ERUI_TEST_CHECK(cache.provider_id_for_line(0U) == "provider.a");
    ERUI_TEST_CHECK(cache.provider_id_for_line(1U) == "provider.b");
    ERUI_TEST_CHECK(cache.provider_id_for_line(2U) == "provider.a");
    ERUI_TEST_CHECK(cache.provider_id_for_line(3U).empty());
    ERUI_TEST_CHECK(cache.publish(
        two_line_plan(), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::already_published);
    ERUI_TEST_CHECK(cache.plan_summary().line_count == 3U);
}

void test_exact_runtime_line_ceiling_transforms() {
    std::vector<TitleGfxRuntimeLine> lines{};
    lines.reserve(erui::gfx::kTitleGfxRuntimeMaximumLines);
    for (std::size_t index = 0;
         index < erui::gfx::kTitleGfxRuntimeMaximumLines;
         ++index) {
        lines.push_back({
            "provider.maximum",
            "Line " + std::to_string(index + 1U),
            std::nullopt,
            TitleGfxCorner::top_right});
    }
    TitleGfxRuntimeCache cache{};
    ERUI_TEST_CHECK(cache.publish(
        std::move(lines), TitleGfxDlcState::absent) ==
        TitleGfxRuntimePublishStatus::published);
    ERUI_TEST_CHECK(cache.plan_summary().line_count ==
        erui::gfx::kTitleGfxRuntimeMaximumLines);
    const auto transformed = cache.prepare(title_fixture());
    ERUI_TEST_CHECK(transformed.success());
    ERUI_TEST_CHECK(transformed.has_replacement());
    ERUI_TEST_CHECK(transformed.after.existing_private_lines ==
        erui::gfx::kTitleGfxRuntimeMaximumLines);
}

void test_transform_cache_identity_and_source_change() {
    const auto source = title_fixture();
    TitleGfxRuntimeCache cache{};
    ERUI_TEST_CHECK(cache.publish(
        two_line_plan(), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::published);

    const TitleGfxRuntimeResult first = cache.prepare(source);
    ERUI_TEST_CHECK(first.success());
    ERUI_TEST_CHECK(first.outcome == TitleGfxRuntimeOutcome::transformed);
    ERUI_TEST_CHECK(first.has_replacement());
    ERUI_TEST_CHECK(!first.cache_hit);
    ERUI_TEST_CHECK(first.after.existing_private_lines == 2U);
    const std::uint8_t* const stable_bytes = first.replacement_bytes;
    const std::size_t stable_size = first.replacement_size;

    const TitleGfxRuntimeResult original_hit = cache.prepare(source);
    ERUI_TEST_CHECK(original_hit.success());
    ERUI_TEST_CHECK(original_hit.cache_hit);
    ERUI_TEST_CHECK(original_hit.source_profile == first.source_profile);
    ERUI_TEST_CHECK(original_hit.replacement_bytes == stable_bytes);
    ERUI_TEST_CHECK(original_hit.replacement_size == stable_size);

    const TitleGfxRuntimeResult replacement_hit = cache.prepare(
        std::span<const std::uint8_t>{stable_bytes, stable_size});
    ERUI_TEST_CHECK(replacement_hit.success());
    ERUI_TEST_CHECK(replacement_hit.cache_hit);
    ERUI_TEST_CHECK(replacement_hit.source_profile == first.source_profile);
    ERUI_TEST_CHECK(replacement_hit.replacement_bytes == stable_bytes);
    ERUI_TEST_CHECK(replacement_hit.replacement_size == stable_size);

    std::vector<std::uint8_t> changed = source;
    changed[changed.size() - 3U] ^= 0x01U;
    const TitleGfxRuntimeResult rejected = cache.prepare(changed);
    ERUI_TEST_CHECK(!rejected.success());
    ERUI_TEST_CHECK(rejected.outcome == TitleGfxRuntimeOutcome::failed);
    ERUI_TEST_CHECK(rejected.error.code ==
        TitleGfxRuntimeErrorCode::source_changed);
    ERUI_TEST_CHECK(!rejected.has_replacement());

    const TitleGfxRuntimeResult still_stable = cache.prepare(source);
    ERUI_TEST_CHECK(still_stable.cache_hit);
    ERUI_TEST_CHECK(still_stable.replacement_bytes == stable_bytes);
}

void test_exact_pretransformed_source_is_unchanged_and_cached() {
    const auto source = title_fixture();
    TitleGfxRuntimeCache producer{};
    ERUI_TEST_CHECK(producer.publish(
        two_line_plan(), TitleGfxDlcState::installed) ==
        TitleGfxRuntimePublishStatus::published);
    const auto produced = producer.prepare(source);
    ERUI_TEST_CHECK(produced.has_replacement());
    const std::vector<std::uint8_t> transformed(
        produced.replacement_bytes,
        produced.replacement_bytes + produced.replacement_size);

    TitleGfxRuntimeCache consumer{};
    ERUI_TEST_CHECK(consumer.publish(
        two_line_plan(), TitleGfxDlcState::installed) ==
        TitleGfxRuntimePublishStatus::published);
    const auto first = consumer.prepare(transformed);
    ERUI_TEST_CHECK(first.success());
    ERUI_TEST_CHECK(first.outcome == TitleGfxRuntimeOutcome::unchanged);
    ERUI_TEST_CHECK(!first.has_replacement());
    ERUI_TEST_CHECK(!first.cache_hit);
    const auto second = consumer.prepare(transformed);
    ERUI_TEST_CHECK(second.success());
    ERUI_TEST_CHECK(second.outcome == TitleGfxRuntimeOutcome::unchanged);
    ERUI_TEST_CHECK(second.cache_hit);
}

void test_failures_are_cached_and_fail_open() {
    TitleGfxRuntimeCache malformed_cache{};
    ERUI_TEST_CHECK(malformed_cache.publish(
        two_line_plan(), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::published);
    const std::vector<std::uint8_t> malformed(12U, 0xA5U);
    const auto malformed_first = malformed_cache.prepare(malformed);
    ERUI_TEST_CHECK(!malformed_first.success());
    ERUI_TEST_CHECK(malformed_first.error.code ==
        TitleGfxRuntimeErrorCode::parse_failed);
    ERUI_TEST_CHECK(!malformed_first.cache_hit);
    ERUI_TEST_CHECK(!malformed_first.has_replacement());
    const auto malformed_second = malformed_cache.prepare(malformed);
    ERUI_TEST_CHECK(!malformed_second.success());
    ERUI_TEST_CHECK(malformed_second.cache_hit);
    ERUI_TEST_CHECK(!malformed_second.has_replacement());

    TitleGfxRuntimeCache incompatible_cache{};
    ERUI_TEST_CHECK(incompatible_cache.publish(
        two_line_plan(), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::published);
    const auto incompatible = incompatible_cache.prepare(title_fixture(false));
    ERUI_TEST_CHECK(!incompatible.success());
    ERUI_TEST_CHECK(incompatible.error.code ==
        TitleGfxRuntimeErrorCode::transform_failed);
    ERUI_TEST_CHECK(!incompatible.error.title_error.success());
    ERUI_TEST_CHECK(!incompatible.has_replacement());
}

void test_input_limit_does_not_poison_cache() {
    TitleGfxRuntimeCache cache{};
    ERUI_TEST_CHECK(cache.publish(
        two_line_plan(), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::published);
    const std::vector<std::uint8_t> oversized(
        erui::gfx::kTitleGfxRuntimeMaximumBytes + 1U, 0U);
    const auto rejected = cache.prepare(oversized);
    ERUI_TEST_CHECK(!rejected.success());
    ERUI_TEST_CHECK(rejected.error.code ==
        TitleGfxRuntimeErrorCode::input_too_large);
    ERUI_TEST_CHECK(!rejected.cache_hit);

    const auto accepted = cache.prepare(title_fixture());
    ERUI_TEST_CHECK(accepted.success());
    ERUI_TEST_CHECK(accepted.outcome == TitleGfxRuntimeOutcome::transformed);
    ERUI_TEST_CHECK(accepted.has_replacement());
}

void test_output_limit_fails_open() {
    TitleGfxRuntimeCache cache{};
    ERUI_TEST_CHECK(cache.publish(
        two_line_plan(), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::published);
    const auto source = padded_title_fixture(
        erui::gfx::kTitleGfxRuntimeMaximumBytes - 8U);
    const auto rejected = cache.prepare(source);
    ERUI_TEST_CHECK(!rejected.success());
    ERUI_TEST_CHECK(rejected.outcome == TitleGfxRuntimeOutcome::failed);
    ERUI_TEST_CHECK(rejected.error.code ==
        TitleGfxRuntimeErrorCode::output_too_large);
    ERUI_TEST_CHECK(!rejected.has_replacement());
}

void test_concurrent_prepare_returns_one_stable_replacement() {
    constexpr std::size_t thread_count = 16U;
    const auto source = title_fixture();
    TitleGfxRuntimeCache cache{};
    ERUI_TEST_CHECK(cache.publish(
        two_line_plan(), TitleGfxDlcState::unknown) ==
        TitleGfxRuntimePublishStatus::published);
    std::array<TitleGfxRuntimeResult, thread_count> results{};
    std::barrier start{static_cast<std::ptrdiff_t>(thread_count)};
    std::vector<std::thread> threads{};
    threads.reserve(thread_count);
    for (std::size_t index = 0; index < thread_count; ++index) {
        threads.emplace_back([&, index] {
            start.arrive_and_wait();
            results[index] = cache.prepare(source);
        });
    }
    for (auto& thread : threads) thread.join();

    const std::uint8_t* stable{};
    std::size_t stable_size{};
    std::size_t first_count{};
    for (const auto& result : results) {
        ERUI_TEST_CHECK(result.success());
        ERUI_TEST_CHECK(result.outcome == TitleGfxRuntimeOutcome::transformed);
        ERUI_TEST_CHECK(result.has_replacement());
        if (!result.cache_hit) ++first_count;
        if (!stable) {
            stable = result.replacement_bytes;
            stable_size = result.replacement_size;
        } else {
            ERUI_TEST_CHECK(result.replacement_bytes == stable);
            ERUI_TEST_CHECK(result.replacement_size == stable_size);
        }
    }
    ERUI_TEST_CHECK(first_count == 1U);
    const auto parsed = GfxDocument::parse(
        std::span<const std::uint8_t>{stable, stable_size});
    ERUI_TEST_CHECK(parsed.success());
    const auto inspection = erui::gfx::inspect_title_gfx(parsed.document);
    ERUI_TEST_CHECK(inspection.success());
    ERUI_TEST_CHECK(inspection.inspection.existing_private_lines == 2U);
}

void test_concurrent_publish_settles_exactly_once() {
    TitleGfxRuntimeCache cache{};
    std::array<TitleGfxRuntimePublishStatus, 2> statuses{};
    std::barrier start{2};
    std::array<std::thread, 2> threads{
        std::thread{[&] {
            start.arrive_and_wait();
            statuses[0] = cache.publish({{
                "provider.a", "A", std::nullopt,
                TitleGfxCorner::top_left}}, TitleGfxDlcState::absent);
        }},
        std::thread{[&] {
            start.arrive_and_wait();
            statuses[1] = cache.publish({{
                "provider.b", "B", TitleGfxRgb{1U, 2U, 3U},
                TitleGfxCorner::bottom_right}}, TitleGfxDlcState::installed);
        }}};
    for (auto& thread : threads) thread.join();

    const std::size_t published = static_cast<std::size_t>(
        statuses[0] == TitleGfxRuntimePublishStatus::published) +
        static_cast<std::size_t>(
            statuses[1] == TitleGfxRuntimePublishStatus::published);
    const std::size_t rejected = static_cast<std::size_t>(
        statuses[0] == TitleGfxRuntimePublishStatus::already_published) +
        static_cast<std::size_t>(
            statuses[1] == TitleGfxRuntimePublishStatus::already_published);
    ERUI_TEST_CHECK(published == 1U);
    ERUI_TEST_CHECK(rejected == 1U);
    const auto summary = cache.plan_summary();
    ERUI_TEST_CHECK(summary.state == TitleGfxRuntimePlanState::ready_lines);
    ERUI_TEST_CHECK(summary.line_count == 1U);
    ERUI_TEST_CHECK(summary.provider_count == 1U);
}

} // namespace

int main(int argc, char** argv) {
    test_exact_path_classifier_and_names();
    test_unrecognized_source_cannot_reuse_the_native_dlc_row();
    test_external_occupancy_is_owned_and_reaches_the_transformer();
    test_pending_and_empty_publication_are_non_mutating();
    test_oversized_rejection_does_not_settle_publication();
    test_pending_prepare_does_not_capture_source();
    test_bounded_plan_wait_times_out_without_settling_publication();
    test_waiting_first_open_observes_plan_and_transforms();
    test_plan_summary_and_publish_once();
    test_exact_runtime_line_ceiling_transforms();
    test_transform_cache_identity_and_source_change();
    test_exact_pretransformed_source_is_unchanged_and_cached();
    test_failures_are_cached_and_fail_open();
    test_input_limit_does_not_poison_cache();
    test_output_limit_fails_open();
    test_concurrent_prepare_returns_one_stable_replacement();
    test_concurrent_publish_settles_exactly_once();
    if (argc > 1) test_real_title_profile(argv[1]);
    std::puts("title GFX runtime tests passed");
    return 0;
}
