#include "menu.hpp"
#include "menu_compiler.hpp"
#include "native_row_presentation.hpp"
#include "row_presentation_state.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr ERUI_ProviderHandle kProvider = 41;
constexpr ERUI_RowHandle kRow = 73;

std::unique_ptr<erui::detail::CompiledMenu> make_menu(bool enabled = true) {
    erui::Menu menu(L"Host", L"Help");
    volatile std::uint8_t value = 1;
    menu.root().add_slider(
        L"Original label", L"Original help", value, {0, 2, 1}, enabled);
    menu.root().identify_last_row(kProvider, kRow);
    return erui::detail::MenuCompiler::compile(menu);
}

} // namespace

int main() {
    using erui::detail::RowPresentationUpdate;
    using erui::native::WidgetTextLayout;
    using erui::native::WidgetTextRole;
    using erui::native::WidgetTextTarget;
    using erui::native::AvailabilityColorTransform;
    using erui::native::AvailabilityTransformObservation;
    using erui::native::AvailabilityTransformOwnershipKey;
    using erui::native::AvailabilityTransformOwnershipState;
    using erui::native::AvailabilityVisualState;

    static_assert(alignof(
        erui::native::AvailabilityColorTransformTransfer) >= 16);
    static_assert(sizeof(
        erui::native::AvailabilityColorTransformTransfer) ==
        sizeof(erui::native::AvailabilityColorTransform));

    const erui::native::AvailabilityColorTransform authored_transform{
        0.8f, 0.6f, 0.4f, 0.75f,
        12.0f, 8.0f, 4.0f, 2.0f};
    const auto dimmed_transform =
        erui::native::compose_availability_color_transform(
            authored_transform,
            AvailabilityVisualState::disabled,
            AvailabilityVisualState::normal);
    assert(dimmed_transform[0] == 0.4f);
    assert(dimmed_transform[1] == 0.3f);
    assert(dimmed_transform[2] == 0.2f);
    assert(dimmed_transform[3] == authored_transform[3]);
    assert(dimmed_transform[4] == 6.0f);
    assert(dimmed_transform[5] == 4.0f);
    assert(dimmed_transform[6] == 2.0f);
    assert(dimmed_transform[7] == authored_transform[7]);
    assert(erui::native::compose_availability_color_transform(
        authored_transform,
        AvailabilityVisualState::normal,
        AvailabilityVisualState::normal) == authored_transform);
    assert(erui::native::compose_availability_color_transform(
        authored_transform,
        AvailabilityVisualState::disabled,
        AvailabilityVisualState::disabled) == authored_transform);
    const auto restored_from_disabled =
        erui::native::compose_availability_color_transform(
            authored_transform,
            AvailabilityVisualState::normal,
            AvailabilityVisualState::disabled);
    assert(restored_from_disabled[0] == 1.6f);
    assert(restored_from_disabled[1] == 1.2f);
    assert(restored_from_disabled[2] == 0.8f);
    assert(restored_from_disabled[3] == authored_transform[3]);

    const AvailabilityTransformOwnershipKey shared_value_key{
        .movie_owner = 0x1000u,
        .value_payload = 0x2000u,
        .movie_epoch = 7u,
    };
    AvailabilityTransformOwnershipState shared_value_state{};
    assert(erui::native::observe_availability_transform_ownership(
        shared_value_state, shared_value_key, authored_transform) ==
        AvailabilityTransformObservation::captured);
    auto disabled_plan =
        erui::native::plan_availability_transform_ownership(
            shared_value_state,
            shared_value_key,
            authored_transform,
            AvailabilityVisualState::disabled,
            AvailabilityVisualState::normal);
    assert(disabled_plan.valid);
    assert(disabled_plan.needs_write);
    assert(disabled_plan.requested == dimmed_transform);
    assert(erui::native::commit_availability_transform_ownership(
        shared_value_state, shared_value_key, disabled_plan.requested));
    assert(shared_value_state.owned);

    // Repeated disabled publication is absolute from the captured base. It
    // must never multiply the transform installed by the previous frame.
    for (int repeat = 0; repeat < 4; ++repeat) {
        assert(erui::native::observe_availability_transform_ownership(
            shared_value_state,
            shared_value_key,
            dimmed_transform) ==
            AvailabilityTransformObservation::retained_installed);
        disabled_plan =
            erui::native::plan_availability_transform_ownership(
                shared_value_state,
                shared_value_key,
                dimmed_transform,
                AvailabilityVisualState::disabled,
                AvailabilityVisualState::normal);
        assert(disabled_plan.valid);
        assert(!disabled_plan.needs_write);
        assert(disabled_plan.requested == dimmed_transform);
        assert(shared_value_state.base == authored_transform);
    }
    auto normal_plan = erui::native::plan_availability_transform_ownership(
        shared_value_state,
        shared_value_key,
        dimmed_transform,
        AvailabilityVisualState::normal,
        AvailabilityVisualState::normal);
    assert(normal_plan.valid);
    assert(normal_plan.needs_write);
    assert(normal_plan.requested == authored_transform);
    assert(erui::native::commit_availability_transform_ownership(
        shared_value_state, shared_value_key, normal_plan.requested));
    assert(!shared_value_state.owned);

    // Alternating native page wrappers are deliberately absent from the key.
    // One shared Scaleform payload therefore retains its original base.
    assert(erui::native::commit_availability_transform_ownership(
        shared_value_state, shared_value_key, dimmed_transform));
    for (const std::uintptr_t page : {0x3000u, 0x4000u, 0x3000u, 0x4000u}) {
        (void)page;
        assert(erui::native::observe_availability_transform_ownership(
            shared_value_state,
            shared_value_key,
            dimmed_transform) ==
            AvailabilityTransformObservation::retained_installed);
        assert(shared_value_state.base == authored_transform);
    }

    // Left labels and right-side slider values retain distinct authored
    // transforms even though one availability decision updates them together.
    const AvailabilityColorTransform label_base{
        1.0f, 0.75f, 0.5f, 0.625f,
        16.0f, 8.0f, 4.0f, 2.0f};
    const AvailabilityColorTransform value_base{
        0.5f, 1.0f, 0.75f, 0.875f,
        2.0f, 4.0f, 8.0f, 1.0f};
    const AvailabilityTransformOwnershipKey label_key{
        .movie_owner = 0x1000u,
        .value_payload = 0x2100u,
        .movie_epoch = 7u,
    };
    const AvailabilityTransformOwnershipKey value_key{
        .movie_owner = 0x1000u,
        .value_payload = 0x2200u,
        .movie_epoch = 7u,
    };
    AvailabilityTransformOwnershipState label_state{};
    AvailabilityTransformOwnershipState value_state{};
    assert(erui::native::observe_availability_transform_ownership(
        label_state, label_key, label_base) ==
        AvailabilityTransformObservation::captured);
    assert(erui::native::observe_availability_transform_ownership(
        value_state, value_key, value_base) ==
        AvailabilityTransformObservation::captured);
    const auto label_disabled =
        erui::native::plan_availability_transform_ownership(
            label_state, label_key, label_base,
            AvailabilityVisualState::disabled,
            AvailabilityVisualState::normal);
    const auto value_disabled =
        erui::native::plan_availability_transform_ownership(
            value_state, value_key, value_base,
            AvailabilityVisualState::disabled,
            AvailabilityVisualState::normal);
    assert(label_disabled.valid && value_disabled.valid);
    assert(label_disabled.requested ==
        erui::native::compose_availability_color_transform(
            label_base,
            AvailabilityVisualState::disabled,
            AvailabilityVisualState::normal));
    assert(value_disabled.requested ==
        erui::native::compose_availability_color_transform(
            value_base,
            AvailabilityVisualState::disabled,
            AvailabilityVisualState::normal));
    assert(label_disabled.requested != value_disabled.requested);
    assert(label_state.base == label_base);
    assert(value_state.base == value_base);
    assert(erui::native::commit_availability_transform_ownership(
        label_state, label_key, label_disabled.requested));
    assert(erui::native::commit_availability_transform_ownership(
        value_state, value_key, value_disabled.requested));
    const auto label_restored =
        erui::native::plan_availability_transform_ownership(
            label_state, label_key, label_disabled.requested,
            AvailabilityVisualState::normal,
            AvailabilityVisualState::normal);
    const auto value_restored =
        erui::native::plan_availability_transform_ownership(
            value_state, value_key, value_disabled.requested,
            AvailabilityVisualState::normal,
            AvailabilityVisualState::normal);
    assert(label_restored.valid && value_restored.valid);
    assert(label_restored.requested == label_base);
    assert(value_restored.requested == value_base);

    // A genuinely different movie/payload identity starts with its own base.
    const AvailabilityTransformOwnershipKey replacement_key{
        .movie_owner = 0x5000u,
        .value_payload = 0x6000u,
        .movie_epoch = 8u,
    };
    const AvailabilityColorTransform replacement_base{
        0.25f, 0.5f, 1.0f, 0.75f,
        1.0f, 2.0f, 4.0f, 3.0f};
    assert(erui::native::observe_availability_transform_ownership(
        shared_value_state, replacement_key, replacement_base) ==
        AvailabilityTransformObservation::recaptured_new_key);
    assert(shared_value_state.key == replacement_key);
    assert(shared_value_state.base == replacement_base);
    assert(shared_value_state.installed == replacement_base);
    assert(!shared_value_state.owned);

    // Slider value/index changes do not participate in display-object
    // ownership and therefore cannot recapture or progressively dim its base.
    const auto replacement_dimmed =
        erui::native::compose_availability_color_transform(
            replacement_base,
            AvailabilityVisualState::disabled,
            AvailabilityVisualState::normal);
    assert(erui::native::commit_availability_transform_ownership(
        shared_value_state, replacement_key, replacement_dimmed));
    for (const std::uint8_t value : {0u, 1u, 9u, 3u, 10u}) {
        (void)value;
        assert(erui::native::observe_availability_transform_ownership(
            shared_value_state,
            replacement_key,
            replacement_dimmed) ==
            AvailabilityTransformObservation::retained_installed);
        assert(shared_value_state.base == replacement_base);
    }

    // Construction-disabled movies round-trip through the inverse visual
    // override without changing either alpha component or accumulating scale.
    const AvailabilityColorTransform construction_disabled_base{
        0.25f, 0.375f, 0.5f, 0.625f,
        1.0f, 2.0f, 4.0f, 8.0f};
    const AvailabilityTransformOwnershipKey construction_disabled_key{
        .movie_owner = 0x7000u,
        .value_payload = 0x8000u,
        .movie_epoch = 9u,
    };
    AvailabilityTransformOwnershipState construction_disabled_state{};
    assert(erui::native::observe_availability_transform_ownership(
        construction_disabled_state,
        construction_disabled_key,
        construction_disabled_base) ==
        AvailabilityTransformObservation::captured);
    const auto forced_normal =
        erui::native::plan_availability_transform_ownership(
            construction_disabled_state,
            construction_disabled_key,
            construction_disabled_base,
            AvailabilityVisualState::normal,
            AvailabilityVisualState::disabled);
    const AvailabilityColorTransform expected_forced_normal{
        0.5f, 0.75f, 1.0f, 0.625f,
        2.0f, 4.0f, 8.0f, 8.0f};
    assert(forced_normal.valid && forced_normal.needs_write);
    assert(forced_normal.requested == expected_forced_normal);
    assert(erui::native::commit_availability_transform_ownership(
        construction_disabled_state,
        construction_disabled_key,
        forced_normal.requested));
    const auto restore_construction_disabled =
        erui::native::plan_availability_transform_ownership(
            construction_disabled_state,
            construction_disabled_key,
            forced_normal.requested,
            AvailabilityVisualState::disabled,
            AvailabilityVisualState::disabled);
    assert(restore_construction_disabled.valid);
    assert(restore_construction_disabled.needs_write);
    assert(restore_construction_disabled.requested ==
        construction_disabled_base);
    assert(erui::native::commit_availability_transform_ownership(
        construction_disabled_state,
        construction_disabled_key,
        restore_construction_disabled.requested));
    const auto forced_normal_again =
        erui::native::plan_availability_transform_ownership(
            construction_disabled_state,
            construction_disabled_key,
            construction_disabled_base,
            AvailabilityVisualState::normal,
            AvailabilityVisualState::disabled);
    assert(forced_normal_again.valid);
    assert(forced_normal_again.requested == expected_forced_normal);

    assert(erui::native::uses_slider_value_availability_transform(
        erui::RowKind::slider));
    assert(erui::native::uses_slider_value_availability_transform(
        erui::RowKind::numeric_slider));
    assert(erui::native::uses_slider_value_availability_transform(
        erui::RowKind::custom_slider));
    assert(!erui::native::uses_slider_value_availability_transform(
        erui::RowKind::button));
    assert(!erui::native::uses_slider_value_availability_transform(
        erui::RowKind::toggle));
    assert(!erui::native::uses_slider_value_availability_transform(
        erui::RowKind::inline_choice));
    assert(!erui::native::uses_slider_value_availability_transform(
        erui::RowKind::popup_choice));
    assert(!erui::native::uses_slider_value_availability_transform(
        erui::RowKind::text_input));
    assert(!erui::native::uses_slider_value_availability_transform(
        erui::RowKind::color_picker));
    assert(!erui::native::uses_slider_value_availability_transform(
        erui::RowKind::submenu));

    // Rollback follows the last successfully applied twin, including when an
    // initially disabled row has since been forced visually normal. Only an
    // uninitialized binding falls back to its construction-selected twin.
    assert(erui::native::select_availability_rollback_visual_state(
        static_cast<std::uint8_t>(AvailabilityVisualState::normal),
        AvailabilityVisualState::disabled) ==
        AvailabilityVisualState::normal);
    assert(erui::native::select_availability_rollback_visual_state(
        static_cast<std::uint8_t>(AvailabilityVisualState::disabled),
        AvailabilityVisualState::normal) ==
        AvailabilityVisualState::disabled);
    assert(erui::native::select_availability_rollback_visual_state(
        0xFFu, AvailabilityVisualState::normal) ==
        AvailabilityVisualState::normal);
    assert(erui::native::select_availability_rollback_visual_state(
        0xFFu, AvailabilityVisualState::disabled) ==
        AvailabilityVisualState::disabled);

    // A shared Text_1/Text child is not enough to identify its semantic role.
    // Family A adds nested Text_0/Text; Family B instead has a direct Text_0
    // row label. This guards the exact ambiguity that once edited the control
    // caption/value instead of the left row label.
    assert(erui::native::select_widget_text_layout(false, false, false) ==
        WidgetTextLayout::none);
    assert(erui::native::select_widget_text_layout(true, false, false) ==
        WidgetTextLayout::none);
    assert(erui::native::select_widget_text_layout(false, true, true) ==
        WidgetTextLayout::none);
    assert(erui::native::select_widget_text_layout(true, true, false) ==
        WidgetTextLayout::family_a);
    assert(erui::native::select_widget_text_layout(true, true, true) ==
        WidgetTextLayout::family_a);
    assert(erui::native::select_widget_text_layout(true, false, true) ==
        WidgetTextLayout::family_b);
    assert(erui::native::select_widget_text_target(
        WidgetTextLayout::none, WidgetTextRole::row_label) ==
        WidgetTextTarget::none);
    assert(erui::native::select_widget_text_target(
        WidgetTextLayout::family_a, WidgetTextRole::row_label) ==
        WidgetTextTarget::nested_text_1);
    assert(erui::native::select_widget_text_target(
        WidgetTextLayout::family_a, WidgetTextRole::control_caption) ==
        WidgetTextTarget::nested_text_0);
    assert(erui::native::select_widget_text_target(
        WidgetTextLayout::family_b, WidgetTextRole::row_label) ==
        WidgetTextTarget::direct_text_0);
    assert(erui::native::select_widget_text_target(
        WidgetTextLayout::family_b, WidgetTextRole::control_caption) ==
        WidgetTextTarget::nested_text_1);

    erui::detail::reset_row_presentation_state();
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Before compile",
            .help = L"Queued before compile",
            .interactive = false,
            .availability_appearance =
                ERUI_AVAILABILITY_APPEARANCE_DISABLED,
        },
    }) == ERUI_OK);

    auto menu = make_menu();
    assert(erui::detail::configure_row_presentation_state(*menu));
    assert(erui::detail::editable_row_presentation_count() == 1);
    assert(menu->root_page().rows[0].presentation_slot == 0);
    assert(erui::detail::row_presentation_updates_pending());

    // Configuration publishes the construction-time availability first.
    // The preconfiguration snapshot remains deferred to the UI boundary.
    const auto availability_before_first_sync =
        erui::detail::row_availability_view(0);
    assert(availability_before_first_sync);
    assert(availability_before_first_sync.interactive);
    assert(availability_before_first_sync.appearance ==
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY);
    const std::uint64_t construction_availability_revision =
        availability_before_first_sync.revision;

    erui::detail::synchronize_pending_row_presentations();
    auto first = erui::detail::row_presentation_view(0);
    assert(first);
    assert(std::wstring(first.label) == L"Before compile");
    assert(std::wstring(first.help) == L"Queued before compile");
    assert(first.caption == nullptr);
    assert(first.help_revision != 0);
    const auto first_availability =
        erui::detail::row_availability_view(0);
    assert(first_availability);
    assert(!first_availability.interactive);
    assert(first_availability.appearance ==
        ERUI_AVAILABILITY_APPEARANCE_DISABLED);
    assert(first_availability.revision >
        construction_availability_revision);
    const wchar_t* const stable_label = first.label;
    const wchar_t* const stable_help = first.help;
    const std::uint64_t first_revision = first.revision;

    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"After compile",
            .help = L"",
            .caption = L"Caption after compile",
            .interactive = true,
            .availability_appearance =
                ERUI_AVAILABILITY_APPEARANCE_NORMAL,
        },
    }) == ERUI_OK);

    // Publication never mutates buffers from the caller thread.
    auto before_sync = erui::detail::row_presentation_view(0);
    assert(before_sync.revision == first_revision);
    assert(std::wstring(before_sync.label) == L"Before compile");
    assert(std::wstring(before_sync.help) == L"Queued before compile");
    assert(before_sync.caption == nullptr);
    const auto availability_before_sync =
        erui::detail::row_availability_view(0);
    assert(availability_before_sync.revision == first_availability.revision);
    assert(!availability_before_sync.interactive);
    assert(availability_before_sync.appearance ==
        ERUI_AVAILABILITY_APPEARANCE_DISABLED);

    erui::detail::synchronize_pending_row_presentations();
    auto second = erui::detail::row_presentation_view(0);
    assert(second.revision > first_revision);
    assert(second.label == stable_label);
    assert(second.help == stable_help);
    assert(second.caption != nullptr);
    assert(std::wstring(second.label) == L"After compile");
    assert(std::wstring(second.help).empty());
    assert(std::wstring(second.caption) == L"Caption after compile");
    assert(second.help_revision > first.help_revision);
    const auto second_availability =
        erui::detail::row_availability_view(0);
    assert(second_availability.revision > first_availability.revision);
    assert(second_availability.interactive);
    assert(second_availability.appearance ==
        ERUI_AVAILABILITY_APPEARANCE_NORMAL);
    const wchar_t* const stable_caption = second.caption;

    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"After compile",
            .help = L"Caption-only generation preserves companions",
            .caption = L"Second caption",
        },
    }) == ERUI_OK);
    const auto caption_before_sync =
        erui::detail::row_presentation_view(0);
    assert(caption_before_sync.revision == second.revision);
    assert(caption_before_sync.caption == stable_caption);
    assert(std::wstring(caption_before_sync.caption) ==
        L"Caption after compile");

    erui::detail::synchronize_pending_row_presentations();
    const auto caption_second = erui::detail::row_presentation_view(0);
    assert(caption_second.revision > second.revision);
    assert(caption_second.label == stable_label);
    assert(caption_second.help == stable_help);
    assert(caption_second.caption == stable_caption);
    assert(std::wstring(caption_second.label) == L"After compile");
    assert(std::wstring(caption_second.help) ==
        L"Caption-only generation preserves companions");
    assert(std::wstring(caption_second.caption) == L"Second caption");
    assert(caption_second.help_revision > second.help_revision);

    // Invalid snapshots are rejected before touching the accepted state.
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"",
            .help = L"invalid",
            .caption = L"Must not publish",
        },
    }) == ERUI_INVALID_ARGUMENT);
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Invalid appearance",
            .help = L"Must not publish",
            .availability_appearance =
                static_cast<ERUI_AvailabilityAppearance>(99u),
        },
    }) == ERUI_INVALID_ARGUMENT);
    erui::detail::synchronize_pending_row_presentations();
    auto after_invalid = erui::detail::row_presentation_view(0);
    assert(after_invalid.revision == caption_second.revision);
    assert(std::wstring(after_invalid.label) == L"After compile");
    assert(std::wstring(after_invalid.caption) == L"Second caption");

    // Caption overrides are optional, but an override itself is non-empty and
    // bounded like the other encoded Scaleform strings.
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Still valid",
            .help = L"Still valid",
            .caption = L"",
        },
    }) == ERUI_INVALID_ARGUMENT);
    std::wstring oversized_caption(
        erui::detail::row_presentation_max_text_units + 1u, L'X');
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Still valid",
            .help = L"Still valid",
            .caption = std::move(oversized_caption),
        },
    }) == ERUI_INVALID_ARGUMENT);

    // A configured store rejects unknown identities without publishing any
    // other record in the same generation.
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Must not publish",
            .help = L"Must not publish",
            .caption = L"Must not publish",
        },
        RowPresentationUpdate{
            .provider = kProvider,
            .row = 999,
            .label = L"Unknown",
            .help = L"Unknown",
            .caption = L"Unknown",
        },
    }) == ERUI_INVALID_HANDLE);
    erui::detail::synchronize_pending_row_presentations();
    auto atomic = erui::detail::row_presentation_view(0);
    assert(atomic.revision == caption_second.revision);
    assert(std::wstring(atomic.label) == L"After compile");
    assert(std::wstring(atomic.caption) == L"Second caption");

    erui::detail::reset_row_presentation_state();
    assert(erui::detail::editable_row_presentation_count() == 0);
    assert(!erui::detail::row_presentation_view(0));
    assert(!erui::detail::row_availability_view(0));

    // A construction-disabled row has an availability snapshot even when it
    // has never received a label/help update.
    auto initially_disabled_menu = make_menu(false);
    assert(erui::detail::configure_row_presentation_state(
        *initially_disabled_menu));
    const auto initially_disabled =
        erui::detail::row_availability_view(0);
    assert(initially_disabled);
    assert(!initially_disabled.interactive);
    assert(initially_disabled.appearance ==
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY);
    assert(!erui::detail::row_presentation_view(0));
    erui::detail::reset_row_presentation_state();

    // A caption accepted before compilation becomes part of the first stable
    // UI-boundary snapshot just like label and help.
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Preconfigured label",
            .help = L"Preconfigured help",
            .caption = L"Preconfigured caption",
            .interactive = false,
            .availability_appearance =
                ERUI_AVAILABILITY_APPEARANCE_DISABLED,
            .refresh_slider_value = true,
        },
    }) == ERUI_OK);
    auto preconfigured_menu = make_menu();
    assert(erui::detail::configure_row_presentation_state(
        *preconfigured_menu));
    erui::detail::synchronize_pending_row_presentations();
    const auto preconfigured = erui::detail::row_presentation_view(0);
    assert(preconfigured);
    assert(std::wstring(preconfigured.label) == L"Preconfigured label");
    assert(std::wstring(preconfigured.help) == L"Preconfigured help");
    assert(std::wstring(preconfigured.caption) == L"Preconfigured caption");
    assert(preconfigured.help_revision != 0);
    assert(preconfigured.slider_value_revision != 0);
    const auto preconfigured_availability =
        erui::detail::row_availability_view(0);
    assert(preconfigured_availability);
    assert(!preconfigured_availability.interactive);
    assert(preconfigured_availability.appearance ==
        ERUI_AVAILABILITY_APPEARANCE_DISABLED);
    erui::detail::reset_row_presentation_state();

    // Help refreshes have their own content generation. Complete snapshots
    // that only alter label, caption, or availability still advance the
    // general revision without scheduling a focused-help republish.
    auto help_generation_menu = make_menu();
    assert(erui::detail::configure_row_presentation_state(
        *help_generation_menu));
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Baseline label",
            .help = L"Original help",
            .caption = L"Baseline caption",
        },
    }) == ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const auto help_baseline =
        erui::detail::row_presentation_view(0);
    assert(help_baseline);
    assert(help_baseline.help_revision == 0);

    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Label and caption only",
            .help = L"Original help",
            .caption = L"Changed caption",
            .interactive = false,
            .availability_appearance =
                ERUI_AVAILABILITY_APPEARANCE_DISABLED,
        },
    }) == ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const auto help_unchanged =
        erui::detail::row_presentation_view(0);
    assert(help_unchanged.revision > help_baseline.revision);
    assert(help_unchanged.help_revision == help_baseline.help_revision);

    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"First queued help",
            .help = L"Queued help A",
            .caption = L"Changed caption",
        },
    }) == ERUI_OK);
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Latest queued help",
            .help = L"Queued help B",
            .caption = L"Changed caption",
        },
    }) == ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const auto help_coalesced =
        erui::detail::row_presentation_view(0);
    assert(std::wstring(help_coalesced.help) == L"Queued help B");
    assert(help_coalesced.help_revision > help_unchanged.help_revision);

    const std::uint64_t coalesced_help_revision =
        help_coalesced.help_revision;
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Transient help",
            .help = L"Transient help C",
            .caption = L"Changed caption",
        },
    }) == ERUI_OK);
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Reverted before sync",
            .help = L"Queued help B",
            .caption = L"Changed caption",
        },
    }) == ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const auto help_reverted =
        erui::detail::row_presentation_view(0);
    assert(std::wstring(help_reverted.help) == L"Queued help B");
    assert(help_reverted.revision > help_coalesced.revision);
    assert(help_reverted.help_revision == coalesced_help_revision);
    erui::detail::reset_row_presentation_state();

    // Formatted-slider repaint requests have an independent generation. An
    // ordinary text/availability edit must not schedule another SliderCtrl
    // refresh, and the generation is never visible before the UI boundary.
    auto slider_generation_menu = make_menu();
    assert(erui::detail::configure_row_presentation_state(
        *slider_generation_menu));
    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Slider baseline",
            .help = L"Slider baseline help",
        },
    }) == ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const auto slider_baseline =
        erui::detail::row_presentation_view(0);
    assert(slider_baseline);
    assert(slider_baseline.slider_value_revision == 0);

    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Slider table B",
            .help = L"Slider table B help",
            .refresh_slider_value = true,
        },
    }) == ERUI_OK);
    const auto slider_before_sync =
        erui::detail::row_presentation_view(0);
    assert(slider_before_sync.slider_value_revision == 0);
    erui::detail::synchronize_pending_row_presentations();
    const auto slider_refreshed =
        erui::detail::row_presentation_view(0);
    assert(slider_refreshed.slider_value_revision != 0);

    assert(erui::detail::submit_row_presentation_updates({
        RowPresentationUpdate{
            .provider = kProvider,
            .row = kRow,
            .label = L"Text-only after slider table",
            .help = L"Text-only help",
        },
    }) == ERUI_OK);
    erui::detail::synchronize_pending_row_presentations();
    const auto slider_not_requeued =
        erui::detail::row_presentation_view(0);
    assert(slider_not_requeued.revision > slider_refreshed.revision);
    assert(slider_not_requeued.slider_value_revision ==
        slider_refreshed.slider_value_revision);
    erui::detail::reset_row_presentation_state();
    return 0;
}
