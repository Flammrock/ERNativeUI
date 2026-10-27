#pragma once

#include "menu_compiler.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace erui::native {

struct GameAddresses;
class ModuleView;

enum class WidgetTextLayout : std::uint8_t {
    none,
    family_a,
    family_b,
};

enum class WidgetTextRole : std::uint8_t {
    row_label,
    control_caption,
};

enum class WidgetTextTarget : std::uint8_t {
    none,
    direct_text_0,
    nested_text_0,
    nested_text_1,
};

enum class AvailabilityVisualState : std::uint8_t {
    normal,
    disabled,
};

using AvailabilityColorTransform = std::array<float, 8>;

// The validated Scaleform color-transform entry points move the two 128-bit
// halves with aligned SIMD instructions. std::array<float, 8> itself only
// guarantees float alignment, so every native call must cross this explicitly
// aligned transfer object rather than a caller-owned array or struct member.
struct alignas(16) AvailabilityColorTransformTransfer final {
    AvailabilityColorTransform value{};
};

static_assert(alignof(AvailabilityColorTransformTransfer) >= 16);
static_assert(sizeof(AvailabilityColorTransformTransfer) ==
    sizeof(AvailabilityColorTransform));

// Scaleform's renderer stores four RGBA multipliers followed by four RGBA
// additive terms. Availability affects RGB only: alpha remains entirely under
// the authored movie's control. A construction-disabled row is already dimmed
// by its native controller, so forcing it visually normal applies the inverse
// scale instead of treating the construction state as normal.
[[nodiscard]] constexpr AvailabilityColorTransform
compose_availability_color_transform(
    AvailabilityColorTransform base,
    AvailabilityVisualState desired,
    AvailabilityVisualState construction) noexcept {
    float factor = 1.0f;
    if (desired != construction) {
        factor = desired == AvailabilityVisualState::disabled ? 0.5f : 2.0f;
    }
    base[0] *= factor;
    base[1] *= factor;
    base[2] *= factor;
    base[4] *= factor;
    base[5] *= factor;
    base[6] *= factor;
    return base;
}

static_assert(compose_availability_color_transform(
    AvailabilityColorTransform{1.0f, 0.8f, 0.6f, 0.75f,
        20.0f, 10.0f, 4.0f, 3.0f},
    AvailabilityVisualState::disabled,
    AvailabilityVisualState::normal) ==
    AvailabilityColorTransform{0.5f, 0.4f, 0.3f, 0.75f,
        10.0f, 5.0f, 2.0f, 3.0f});

// Transform ownership follows the exact Scaleform display object, not the
// native page/controller wrapper that happens to expose it. A movie can reuse
// one display object while rebuilding those wrappers during tab navigation.
struct AvailabilityTransformOwnershipKey final {
    std::uintptr_t movie_owner{};
    std::uintptr_t value_payload{};
    std::uint64_t movie_epoch{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return movie_owner != 0 && value_payload != 0 && movie_epoch != 0;
    }

    [[nodiscard]] constexpr bool operator==(
        const AvailabilityTransformOwnershipKey& other) const noexcept {
        return movie_owner == other.movie_owner &&
            value_payload == other.value_payload &&
            movie_epoch == other.movie_epoch;
    }
};

struct AvailabilityTransformOwnershipState final {
    AvailabilityTransformOwnershipKey key{};
    AvailabilityColorTransform base{};
    AvailabilityColorTransform installed{};
    bool initialized{};
    bool owned{};
};

enum class AvailabilityTransformObservation : std::uint8_t {
    invalid,
    captured,
    recaptured_new_key,
    retained_base,
    retained_installed,
    conflict,
};

struct AvailabilityTransformOwnershipPlan final {
    AvailabilityColorTransform requested{};
    bool valid{};
    bool needs_write{};
};

[[nodiscard]] constexpr AvailabilityTransformObservation
observe_availability_transform_ownership(
    AvailabilityTransformOwnershipState& state,
    AvailabilityTransformOwnershipKey key,
    const AvailabilityColorTransform& current) noexcept {
    if (!key.valid()) return AvailabilityTransformObservation::invalid;
    if (!state.initialized) {
        state = {
            .key = key,
            .base = current,
            .installed = current,
            .initialized = true,
            .owned = false,
        };
        return AvailabilityTransformObservation::captured;
    }
    if (!(state.key == key)) {
        state = {
            .key = key,
            .base = current,
            .installed = current,
            .initialized = true,
            .owned = false,
        };
        return AvailabilityTransformObservation::recaptured_new_key;
    }
    if (current == state.base) {
        return AvailabilityTransformObservation::retained_base;
    }
    if (state.owned && current == state.installed) {
        return AvailabilityTransformObservation::retained_installed;
    }
    return AvailabilityTransformObservation::conflict;
}

[[nodiscard]] constexpr AvailabilityTransformOwnershipPlan
plan_availability_transform_ownership(
    const AvailabilityTransformOwnershipState& state,
    AvailabilityTransformOwnershipKey key,
    const AvailabilityColorTransform& current,
    AvailabilityVisualState desired,
    AvailabilityVisualState construction) noexcept {
    AvailabilityTransformOwnershipPlan plan{};
    if (!state.initialized || !(state.key == key) ||
        (current != state.base &&
         (!state.owned || current != state.installed))) {
        return plan;
    }
    plan.requested = compose_availability_color_transform(
        state.base, desired, construction);
    plan.valid = true;
    plan.needs_write = current != plan.requested;
    return plan;
}

[[nodiscard]] constexpr bool commit_availability_transform_ownership(
    AvailabilityTransformOwnershipState& state,
    AvailabilityTransformOwnershipKey key,
    const AvailabilityColorTransform& installed) noexcept {
    if (!state.initialized || !(state.key == key)) return false;
    state.installed = installed;
    state.owned = installed != state.base;
    return true;
}

[[nodiscard]] constexpr bool uses_slider_value_availability_transform(
    RowKind kind) noexcept {
    return kind == RowKind::slider || kind == RowKind::numeric_slider ||
        kind == RowKind::custom_slider;
}

[[nodiscard]] constexpr AvailabilityVisualState
select_availability_visual_state(
    ERUI_AvailabilityAppearance appearance,
    bool interactive) noexcept {
    if (appearance == ERUI_AVAILABILITY_APPEARANCE_NORMAL) {
        return AvailabilityVisualState::normal;
    }
    if (appearance == ERUI_AVAILABILITY_APPEARANCE_DISABLED) {
        return AvailabilityVisualState::disabled;
    }
    return interactive
        ? AvailabilityVisualState::normal
        : AvailabilityVisualState::disabled;
}

static_assert(select_availability_visual_state(
    ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY,
    true) == AvailabilityVisualState::normal);
static_assert(select_availability_visual_state(
    ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY,
    false) == AvailabilityVisualState::disabled);
static_assert(select_availability_visual_state(
    ERUI_AVAILABILITY_APPEARANCE_NORMAL,
    false) == AvailabilityVisualState::normal);
static_assert(select_availability_visual_state(
    ERUI_AVAILABILITY_APPEARANCE_DISABLED,
    true) == AvailabilityVisualState::disabled);

// A failed two-object visibility update must restore the state that was
// actually visible before the attempt. The construction-selected twin is only
// the fallback before this binding has recorded a successful application.
[[nodiscard]] constexpr AvailabilityVisualState
select_availability_rollback_visual_state(
    std::uint8_t applied_appearance,
    AvailabilityVisualState construction_appearance) noexcept {
    if (applied_appearance ==
        static_cast<std::uint8_t>(AvailabilityVisualState::normal)) {
        return AvailabilityVisualState::normal;
    }
    if (applied_appearance ==
        static_cast<std::uint8_t>(AvailabilityVisualState::disabled)) {
        return AvailabilityVisualState::disabled;
    }
    return construction_appearance;
}

// The two supported movie families contain the same nested Text_1/Text path,
// but assign it opposite semantic roles. Select a layout only from a complete
// structural signature; a single resolvable path is intentionally ambiguous.
[[nodiscard]] constexpr WidgetTextLayout select_widget_text_layout(
    bool common_nested_text,
    bool family_a_nested_caption,
    bool family_b_direct_label) noexcept {
    if (!common_nested_text) return WidgetTextLayout::none;
    if (family_a_nested_caption) return WidgetTextLayout::family_a;
    if (family_b_direct_label) return WidgetTextLayout::family_b;
    return WidgetTextLayout::none;
}

// Family A and Family B deliberately reverse the semantic roles of their
// nested text fields. Keep this mapping separate from path resolution so it
// can be regression-tested without a live Scaleform movie.
[[nodiscard]] constexpr WidgetTextTarget select_widget_text_target(
    WidgetTextLayout layout,
    WidgetTextRole role) noexcept {
    switch (layout) {
    case WidgetTextLayout::family_a:
        return role == WidgetTextRole::row_label
            ? WidgetTextTarget::nested_text_1
            : WidgetTextTarget::nested_text_0;
    case WidgetTextLayout::family_b:
        return role == WidgetTextRole::row_label
            ? WidgetTextTarget::direct_text_0
            : WidgetTextTarget::nested_text_1;
    default:
        return WidgetTextTarget::none;
    }
}

struct RowPresentationMaterializationTicket {
    void* page{};
    std::size_t presentation_slot{detail::invalid_compiled_index};
    std::size_t previous_count{};
    std::uint64_t page_epoch{};
    bool armed{};
};

// A page owns at most one deferred selected-help publication. Newer help
// generations replace an older request while a modal or visibility rebuild
// blocks publication. Every pointer is revalidated against the page epoch and
// physical binding before it can enter native code.
struct SelectedHelpRefreshTicket {
    void* page{};
    void* native_row{};
    const wchar_t* source{};
    std::size_t physical_index{detail::invalid_compiled_index};
    std::size_t presentation_slot{detail::invalid_compiled_index};
    std::uint64_t page_epoch{};
    std::uint64_t help_revision{};
    bool armed{};
};

enum class SelectedHelpRefreshDecision : std::uint8_t {
    none,
    defer,
    discard,
    publish,
};

// Native-free lifecycle policy used by the bridge and unit tests. Selection
// mismatch is consumed: moving focus already asks Elden Ring to present that
// row's current help source, so retrying an old request would be incorrect.
[[nodiscard]] constexpr SelectedHelpRefreshDecision
select_help_refresh_decision(
    const SelectedHelpRefreshTicket& ticket,
    bool blocked,
    std::uint64_t current_page_epoch,
    std::uint32_t selected_count,
    std::uint32_t selected_index,
    std::size_t maximum_rows = 16) noexcept {
    if (!ticket.armed) return SelectedHelpRefreshDecision::none;
    if (blocked) return SelectedHelpRefreshDecision::defer;
    if (!ticket.page || !ticket.native_row || !ticket.source ||
        ticket.page_epoch == 0 || ticket.help_revision == 0 ||
        ticket.physical_index == detail::invalid_compiled_index ||
        ticket.presentation_slot == detail::invalid_compiled_index ||
        ticket.page_epoch != current_page_epoch || selected_count == 0 ||
        selected_count > maximum_rows || selected_index >= selected_count ||
        selected_index != ticket.physical_index) {
        return SelectedHelpRefreshDecision::discard;
    }
    return SelectedHelpRefreshDecision::publish;
}

// Exact-build availability check used before API provider registration opens.
[[nodiscard]] bool native_row_presentation_build_supported(
    const ModuleView& game) noexcept;

// Exact-build gate for API 1.4 typed-row availability. This is deliberately
// separate from ordinary label/help presentation so a future executable may
// retain text updates without silently inheriting an unproved +0x80 or
// Scaleform-presentation contract.
[[nodiscard]] bool native_row_availability_build_supported(
    const ModuleView& game) noexcept;

// Starts a new physical-page binding epoch. Existing bindings for the same
// address are retired without dereferencing them.
void begin_native_row_presentation_page(
    void* page,
    const GameAddresses& addresses) noexcept;

[[nodiscard]] RowPresentationMaterializationTicket
begin_native_row_presentation_materialization(
    void* page,
    const GameAddresses& addresses,
    const detail::CompiledRow& row) noexcept;

void complete_native_row_presentation_materialization(
    void* page,
    const GameAddresses& addresses,
    const detail::CompiledRow& row,
    RowPresentationMaterializationTicket ticket,
    bool constructor_succeeded) noexcept;

// Synchronizes only bindings owned by the exact page supplied by the game's
// page-frame callback. The normal hook calls this after the native frame so
// action-like widgets cannot overwrite edited text and native controllers
// cannot republish their construction-selected availability presentation. A paused
// frame may also call it directly while a native modal dialog owns the update
// path.
[[nodiscard]] SelectedHelpRefreshTicket apply_native_row_presentations(
    void* page,
    const GameAddresses& addresses) noexcept;

// Consumes an exact pending ticket before directly invoking the validated
// native selected-help presenter. The call always occurs outside the
// presentation mutex and only from the page-frame UI thread.
void publish_native_selected_help(
    SelectedHelpRefreshTicket ticket,
    const GameAddresses& addresses) noexcept;

// Installs the desired EditProperty eligibility byte before Elden Ring's
// native page frame consumes it for focus, activation, and hit testing.
void apply_native_row_availability_pre_frame(
    void* page,
    const GameAddresses& addresses) noexcept;

void retire_native_row_presentation_page(
    void* page,
    const GameAddresses& addresses) noexcept;
void reset_native_row_presentations(
    const GameAddresses& addresses) noexcept;

} // namespace erui::native
