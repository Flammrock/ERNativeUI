#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace erui::detail {

enum class TabPointerDwellEdge : std::uint8_t {
    none,
    previous,
    next,
};

enum class TabPointerDwellPhase : std::uint8_t {
    idle,
    awaiting_commit,
    cooldown,
    repeat_due,
};

struct TabPointerDwellTiming {
    std::uint64_t initial_delay_ms{250};
    std::uint64_t repeat_delay_ms{200};
    std::uint64_t repeat_decay_ms{20};
    std::uint64_t minimum_repeat_delay_ms{50};
};

struct TabPointerDwellUpdate {
    bool suppress_natural_selection{};
    bool repeat_due{};
    bool cancelled{};
    bool position_changed{};
    bool deadline_restarted{};
};

// Pure mouse-edge dwell policy. Native cursor sampling, tab selection, and
// viewport mutation stay in the adapter; this class owns only timing/state.
//
// The first native hover remains immediate. Once that native transition
// commits, the initial delay gates the first automatic step. Later committed
// automatic steps use repeat_delay - n * repeat_decay, bounded by the
// configured minimum repeat delay. A minimum above the starting repeat delay
// is normalized down to that starting delay, so malformed configuration can
// never make an accelerated repeat slower than its base cadence.
// Genuine pointer motion clears that acceleration streak and starts a fresh
// initial delay. A zero decay therefore means a fixed repeat cadence; it does
// not disable dwell.
class TabPointerDwell {
public:
    static constexpr TabPointerDwellTiming default_timing{};
    static constexpr std::size_t invalid_physical_slot =
        std::numeric_limits<std::size_t>::max();

    constexpr TabPointerDwell() noexcept = default;

    explicit constexpr TabPointerDwell(
        TabPointerDwellTiming timing) noexcept
        : timing_(normalize_timing(timing)) {}

    // Fixed-cadence shorthand retained for focused tests.
    explicit constexpr TabPointerDwell(
        std::uint64_t fixed_delay_ms) noexcept
        : timing_{fixed_delay_ms, fixed_delay_ms, 0, fixed_delay_ms} {}

    void reset() noexcept {
        phase_ = TabPointerDwellPhase::idle;
        edge_ = TabPointerDwellEdge::none;
        physical_slot_ = invalid_physical_slot;
        x_bits_ = 0;
        y_bits_ = 0;
        cooldown_started_ms_ = 0;
        deadline_ms_ = 0;
        current_delay_ms_ = 0;
        automatic_repeats_committed_ = 0;
        transition_is_automatic_repeat_ = false;
        motion_during_transition_ = false;
    }

    void begin_transition(
        TabPointerDwellEdge edge,
        std::size_t physical_slot,
        std::uint32_t x_bits,
        std::uint32_t y_bits) noexcept {
        if (edge == TabPointerDwellEdge::none ||
            physical_slot == invalid_physical_slot) {
            reset();
            return;
        }
        if (active()) return;
        phase_ = TabPointerDwellPhase::awaiting_commit;
        edge_ = edge;
        physical_slot_ = physical_slot;
        x_bits_ = x_bits;
        y_bits_ = y_bits;
        deadline_ms_ = 0;
        current_delay_ms_ = 0;
        automatic_repeats_committed_ = 0;
        transition_is_automatic_repeat_ = false;
        motion_during_transition_ = false;
    }

    void transition_committed(
        std::uint64_t now_ms,
        bool has_more_in_direction) noexcept {
        if (phase_ != TabPointerDwellPhase::awaiting_commit ||
            !has_more_in_direction) {
            reset();
            return;
        }

        std::uint64_t delay = timing_.initial_delay_ms;
        if (transition_is_automatic_repeat_ &&
            !motion_during_transition_) {
            delay = repeat_delay_for(automatic_repeats_committed_);
            if (automatic_repeats_committed_ !=
                std::numeric_limits<std::uint64_t>::max()) {
                ++automatic_repeats_committed_;
            }
        } else {
            automatic_repeats_committed_ = 0;
        }

        transition_is_automatic_repeat_ = false;
        motion_during_transition_ = false;
        start_cooldown(now_ms, delay);
    }

    void transition_failed() noexcept { reset(); }
    void controller_navigation() noexcept { reset(); }

    void repeat_dispatched() noexcept {
        if (phase_ == TabPointerDwellPhase::repeat_due) {
            phase_ = TabPointerDwellPhase::awaiting_commit;
            deadline_ms_ = 0;
            current_delay_ms_ = 0;
            transition_is_automatic_repeat_ = true;
            motion_during_transition_ = false;
        }
    }

    [[nodiscard]] TabPointerDwellUpdate observe_natural(
        std::int32_t hit,
        std::uint32_t x_bits,
        std::uint32_t y_bits,
        std::uint64_t now_ms) noexcept {
        TabPointerDwellUpdate update = observe_sample(
            hit, x_bits, y_bits, now_ms);
        update.suppress_natural_selection = active();
        update.repeat_due = repeat_due();
        return update;
    }

    [[nodiscard]] TabPointerDwellUpdate observe_poll(
        std::int32_t hit,
        std::uint32_t x_bits,
        std::uint32_t y_bits,
        std::uint64_t now_ms) noexcept {
        TabPointerDwellUpdate update = observe_sample(
            hit, x_bits, y_bits, now_ms);
        if (phase_ == TabPointerDwellPhase::cooldown) {
            if (now_ms < cooldown_started_ms_) {
                start_cooldown(now_ms, current_delay_ms_);
                update.deadline_restarted = true;
            } else if (now_ms - cooldown_started_ms_ >= current_delay_ms_) {
                phase_ = TabPointerDwellPhase::repeat_due;
            }
        }
        update.repeat_due = repeat_due();
        return update;
    }

    [[nodiscard]] bool active() const noexcept {
        return phase_ != TabPointerDwellPhase::idle;
    }

    [[nodiscard]] bool repeat_due() const noexcept {
        return phase_ == TabPointerDwellPhase::repeat_due;
    }

    [[nodiscard]] TabPointerDwellPhase phase() const noexcept {
        return phase_;
    }

    [[nodiscard]] TabPointerDwellEdge edge() const noexcept {
        return edge_;
    }

    [[nodiscard]] std::size_t physical_slot() const noexcept {
        return physical_slot_;
    }

    [[nodiscard]] std::uint32_t x_bits() const noexcept { return x_bits_; }
    [[nodiscard]] std::uint32_t y_bits() const noexcept { return y_bits_; }
    [[nodiscard]] std::uint64_t deadline_ms() const noexcept {
        return deadline_ms_;
    }
    [[nodiscard]] std::uint64_t current_delay_ms() const noexcept {
        return current_delay_ms_;
    }
    [[nodiscard]] std::uint64_t automatic_repeats_committed() const noexcept {
        return automatic_repeats_committed_;
    }
    [[nodiscard]] TabPointerDwellTiming timing() const noexcept {
        return timing_;
    }

private:
    [[nodiscard]] static constexpr TabPointerDwellTiming normalize_timing(
        TabPointerDwellTiming timing) noexcept {
        if (timing.minimum_repeat_delay_ms > timing.repeat_delay_ms) {
            timing.minimum_repeat_delay_ms = timing.repeat_delay_ms;
        }
        return timing;
    }

    [[nodiscard]] TabPointerDwellUpdate observe_sample(
        std::int32_t hit,
        std::uint32_t x_bits,
        std::uint32_t y_bits,
        std::uint64_t now_ms) noexcept {
        TabPointerDwellUpdate update{};
        if (!active()) return update;

        if (hit < 0 || static_cast<std::size_t>(hit) != physical_slot_) {
            reset();
            update.cancelled = true;
            return update;
        }

        if (x_bits == x_bits_ && y_bits == y_bits_) return update;

        x_bits_ = x_bits;
        y_bits_ = y_bits;
        automatic_repeats_committed_ = 0;
        update.position_changed = true;
        if (phase_ == TabPointerDwellPhase::cooldown ||
            phase_ == TabPointerDwellPhase::repeat_due) {
            transition_is_automatic_repeat_ = false;
            motion_during_transition_ = false;
            start_cooldown(now_ms, timing_.initial_delay_ms);
            update.deadline_restarted = true;
        } else if (phase_ == TabPointerDwellPhase::awaiting_commit) {
            motion_during_transition_ = true;
        }
        return update;
    }

    [[nodiscard]] constexpr std::uint64_t repeat_delay_for(
        std::uint64_t completed_repeat_index) const noexcept {
        if (timing_.repeat_decay_ms == 0) {
            return timing_.repeat_delay_ms;
        }

        const std::uint64_t decay_range =
            timing_.repeat_delay_ms - timing_.minimum_repeat_delay_ms;
        if (completed_repeat_index >
            decay_range / timing_.repeat_decay_ms) {
            return timing_.minimum_repeat_delay_ms;
        }
        const std::uint64_t decayed = timing_.repeat_delay_ms -
            completed_repeat_index * timing_.repeat_decay_ms;
        return decayed < timing_.minimum_repeat_delay_ms
            ? timing_.minimum_repeat_delay_ms
            : decayed;
    }

    void start_cooldown(
        std::uint64_t now_ms,
        std::uint64_t delay_ms) noexcept {
        phase_ = TabPointerDwellPhase::cooldown;
        cooldown_started_ms_ = now_ms;
        current_delay_ms_ = delay_ms;
        deadline_ms_ = add_delay(now_ms, delay_ms);
    }

    [[nodiscard]] static std::uint64_t add_delay(
        std::uint64_t now_ms,
        std::uint64_t delay_ms) noexcept {
        const std::uint64_t maximum =
            std::numeric_limits<std::uint64_t>::max();
        return delay_ms > maximum - now_ms
            ? maximum
            : now_ms + delay_ms;
    }

    TabPointerDwellTiming timing_{default_timing};
    TabPointerDwellPhase phase_{TabPointerDwellPhase::idle};
    TabPointerDwellEdge edge_{TabPointerDwellEdge::none};
    std::size_t physical_slot_{invalid_physical_slot};
    std::uint32_t x_bits_{};
    std::uint32_t y_bits_{};
    std::uint64_t cooldown_started_ms_{};
    std::uint64_t deadline_ms_{};
    std::uint64_t current_delay_ms_{};
    std::uint64_t automatic_repeats_committed_{};
    bool transition_is_automatic_repeat_{};
    bool motion_during_transition_{};
};

} // namespace erui::detail
