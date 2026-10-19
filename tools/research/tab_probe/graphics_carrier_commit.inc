PendingTransitionCommitResult commit_pending_transition(
    void* top) noexcept {
    PendingTransition& pending = g_runtime.pending;
    const auto freeze_destructive_transaction =
        [&pending](const char* reason) noexcept {
            if (!g_in_place_rgba8_rebind_transaction.active) return false;
            const std::uint64_t ticket = pending.pointer_dwell_ticket;
            (void)freeze_active_two_phase_rgba8_transaction(reason);
            pending = {};
            fail_pointer_dwell_transition(ticket, reason);
            return true;
        };
    if (!pending.active || pending.top != top || !validate_top(top) ||
        !records_match_mapping(top, g_runtime.viewport)) {
        (void)freeze_destructive_transaction(
            "pending transition validation or source mapping failed");
        return PendingTransitionCommitResult::failed;
    }
    if (g_in_place_rgba8_rebind_transaction.active &&
        !g_in_place_rgba8_rebind_transaction.failed &&
        g_in_place_rgba8_rebind_transaction.stage ==
            InPlaceRgba8RebindStage::waiting_retirement) {
        return PendingTransitionCommitResult::waiting;
    }

    const bool repaint_required =
        pending.candidate.mapping_generation() !=
            g_runtime.viewport.mapping_generation() ||
        pending.candidate.window_start() !=
            g_runtime.viewport.window_start() ||
        tab_overflow_indicators_changed(
            g_runtime.viewport, pending.candidate);
    if (pending.diagnose_wrap) {
        log_wrap_panel_state(top, "commit-before-remap",
            pending.previous_logical, pending.desired_logical);
    }

    g_mutating_viewport = true;
    if (repaint_required) {
        ResolvedTabLayers layers{};
        if (!resolve_tab_layers(top, layers)) {
            g_mutating_viewport = false;
            log_failure(
                "viewport remap aborted before mutation: dual-layer cells unavailable");
            if (freeze_destructive_transaction(
                    "dual-layer cells unavailable after phase-A clear")) {
                return PendingTransitionCommitResult::failed;
            }
            abandon_pending_to_native_selection();
            return PendingTransitionCommitResult::failed;
        }
        const bool selector_already_relocated =
            g_in_place_rgba8_rebind_transaction.active &&
            (g_in_place_rgba8_rebind_transaction.stage ==
                    InPlaceRgba8RebindStage::ready_to_repaint ||
                g_in_place_rgba8_rebind_transaction.stage ==
                    InPlaceRgba8RebindStage::repainting);
        if (!selector_already_relocated &&
            pending.desired_physical != pending.native_physical &&
            !invoke_selector_select(top, pending.desired_physical)) {
            g_mutating_viewport = false;
            destroy_tab_layers(layers);
            log_failure(
                "viewport remap aborted: selector refused physical=%u",
                static_cast<unsigned>(pending.desired_physical));
            if (freeze_destructive_transaction(
                    "selector relocation failed during transaction")) {
                return PendingTransitionCommitResult::failed;
            }
            abandon_pending_to_native_selection();
            return PendingTransitionCommitResult::failed;
        }
        if (!apply_mapping(top, pending.candidate, layers)) {
            if (freeze_destructive_transaction(
                    "two-phase Shape detach/target repaint failed")) {
                g_mutating_viewport = false;
                return PendingTransitionCommitResult::failed;
            }
            if (pending.desired_physical != pending.native_physical) {
                (void)invoke_selector_select(
                    top, pending.native_physical);
            }
            g_mutating_viewport = false;
            log_failure("viewport remap record rewrite/repaint failed");
            abandon_pending_to_native_selection();
            return PendingTransitionCommitResult::failed;
        }
        if (g_in_place_rgba8_rebind_transaction.active &&
            !g_in_place_rgba8_rebind_transaction.failed &&
            g_in_place_rgba8_rebind_transaction.stage ==
                InPlaceRgba8RebindStage::waiting_retirement) {
            g_mutating_viewport = false;
            return PendingTransitionCommitResult::waiting;
        }
    } else {
        // A content-only route transaction must leave every record and icon
        // byte untouched. The native selector already points at its target.
        if (pending.candidate.mapping_generation() !=
                g_runtime.viewport.mapping_generation() ||
            pending.candidate.window_start() !=
                g_runtime.viewport.window_start() ||
            tab_overflow_indicators_changed(
                g_runtime.viewport, pending.candidate)) {
            g_mutating_viewport = false;
            const std::uint64_t ticket = pending.pointer_dwell_ticket;
            pending = {};
            fail_pointer_dwell_transition(
                ticket, "content-only transaction changed presentation");
            return PendingTransitionCommitResult::failed;
        }
        log_line(
            "graphicsCarrier contentOnly logical=%zu->%zu window=%zu generation=%llu iconRepaint=0 recordRewrite=0",
            pending.previous_logical, pending.desired_logical,
            g_runtime.viewport.window_start(),
            static_cast<unsigned long long>(
                g_runtime.viewport.mapping_generation()));
    }

    if (pending.diagnose_wrap) {
        log_wrap_panel_state(top, "commit-after-mapping",
            pending.previous_logical, pending.desired_logical);
    }

    const bool route_transition =
        pending.previous_logical != pending.desired_logical &&
        (logical_uses_graphics_carrier(pending.previous_logical) ||
            logical_uses_graphics_carrier(pending.desired_logical));
    if (pending.provisional_category != pending.desired_category ||
        route_transition ||
        pending.previous_logical != pending.desired_logical) {
        const bool custom_route_transition =
            is_custom_logical(pending.previous_logical) ||
            is_custom_logical(pending.desired_logical);
        invoke_switch_panel(top, pending.desired_category,
            custom_route_transition
                ? PanelStateIsolationReason::corrective
                : PanelStateIsolationReason::none);
        if (!g_research_switch_succeeded) {
            const std::uint64_t ticket = pending.pointer_dwell_ticket;
            if (repaint_required) {
                // Records/icons/selector now describe the candidate. Publish
                // it before freezing further carrier transitions.
                g_runtime.viewport = pending.candidate;
                pending = {};
                g_mutating_viewport = false;
                fail_pointer_dwell_transition(
                    ticket, "carrier switch failed after viewport repaint");
                return PendingTransitionCommitResult::failed;
            }
            const std::uint32_t old_physical =
                static_cast<std::uint32_t>(
                    g_runtime.viewport.selected_physical());
            const bool restored =
                invoke_selector_select(top, old_physical);
            std::uint32_t observed{};
            const bool exact = restored &&
                read_selected_physical(top, observed) &&
                observed == old_physical;
            pending = {};
            g_mutating_viewport = false;
            fail_pointer_dwell_transition(
                ticket, "content-only carrier switch failed");
            log_failure(
                "Graphics carrier content-only failure selectorRestore={call:%u observed:%u exact:%u}; custom transitions frozen, native Back remains live",
                restored ? 1u : 0u, static_cast<unsigned>(observed),
                exact ? 1u : 0u);
            return PendingTransitionCommitResult::failed;
        }
    }

    log_research_carrier_selection(top, "commit",
        pending.desired_logical, pending.desired_category);
    if (pending.diagnose_wrap) {
        log_wrap_panel_state(top, "commit-after-switch",
            pending.previous_logical, pending.desired_logical);
    }
    invoke_refresh_title(top);

    const std::uint32_t event =
        g_transition_log_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (event <= kMaximumTransitionLogs) {
        log_line(
            "remap[%u] kind=%s nativePhysical=%u desiredPhysical=%u provisionalLogical=%zu desiredLogical=%zu category=%d->%d window=%zu->%zu generation=%llu",
            static_cast<unsigned>(event),
            repaint_required ? "presentation" : "content-only",
            static_cast<unsigned>(pending.native_physical),
            static_cast<unsigned>(pending.desired_physical),
            pending.provisional_logical, pending.desired_logical,
            pending.provisional_category, pending.desired_category,
            g_runtime.viewport.window_start(),
            pending.candidate.window_start(),
            static_cast<unsigned long long>(
                pending.candidate.mapping_generation()));
    }
    const std::uint64_t ticket = pending.pointer_dwell_ticket;
    g_runtime.viewport = pending.candidate;
    pending = {};
    g_mutating_viewport = false;
    finish_pointer_dwell_transition(
        ticket, repaint_required
            ? "viewport remap committed"
            : "content-only carrier route committed");
    return PendingTransitionCommitResult::committed;
}

