constexpr std::size_t kNativeGraphicsLogical = 7;
constexpr std::size_t kNativeGraphicsRoute = kCustomTabCount;

constexpr bool logical_uses_graphics_carrier(
    std::size_t logical) noexcept {
    return logical == kNativeGraphicsLogical ||
        (logical >= kPhysicalTabCount && logical < kLogicalTabCount);
}

constexpr bool is_custom_logical(std::size_t logical) noexcept {
    return logical >= kPhysicalTabCount && logical < kLogicalTabCount;
}

constexpr std::size_t graphics_route_for_logical(
    std::size_t logical) noexcept {
    return logical == kNativeGraphicsLogical
        ? kNativeGraphicsRoute
        : logical - kPhysicalTabCount;
}

// Called synchronously by the host only after exact identity, age and native
// retirement validation. Category 5 is parking; category 8 is the carrier.
bool __cdecl switch_research_carrier_isolated(
    void* composite,
    std::int32_t category) noexcept {
    const auto original = g_original_research_switch.load(
        std::memory_order_acquire);
    if (!original || !composite || (category != 5 && category != 8) ||
        !g_research_explicit_switch || !g_runtime.pending.active ||
        static_cast<std::byte*>(g_runtime.top) + kCompositeOffset !=
            composite) return false;
    auto* const bytes = static_cast<std::byte*>(composite);
    void* source{};
    void* cached_target{};
    if (!guarded_read(bytes + kCompositeCurrentPanelOffset, source) ||
        !guarded_read(
            bytes + kCompositePanelCacheOffset +
                category * sizeof(void*),
            cached_target)) return false;
    g_state_isolation_expectation = {};
    const bool true_custom_route_involved =
        is_custom_logical(g_runtime.pending.previous_logical) ||
        is_custom_logical(g_runtime.pending.desired_logical);
    const bool native_copy_would_run = source && source != cached_target;
    const bool suppress_copy = native_copy_would_run &&
        (category == 5 || true_custom_route_involved);
    const bool armed = suppress_copy && begin_panel_state_isolation(
        g_runtime.top, category,
        PanelStateIsolationReason::corrective);
    if (suppress_copy && !armed) return false;
    original(composite, category);
    const bool copy_policy_exact = !suppress_copy ||
        (g_state_isolation_expectation.active &&
            g_state_isolation_expectation.consumed);
    end_panel_state_isolation(armed);
    void* current{};
    void* target{};
    const bool exact = copy_policy_exact &&
        guarded_read(bytes + kCompositeCurrentPanelOffset, current) &&
        guarded_read(
            bytes + kCompositePanelCacheOffset +
                category * sizeof(void*),
            target) && current && current == target;
    log_line(
        "graphicsCarrier nativeSwitch category=%d source=%p target=%p currentExact=%u stateCopyPolicy=%s customBoundary=%u frame=%llu",
        category, source, target, exact ? 1u : 0u,
        suppress_copy ? "suppressed" : "native-preserved",
        true_custom_route_involved ? 1u : 0u,
        static_cast<unsigned long long>(
            g_research_completed_composite_frames));
    return exact;
}

void __fastcall research_switch_panel_detour(
    void* composite,
    std::int32_t category) noexcept {
    const auto original = g_original_research_switch.load(
        std::memory_order_acquire);
    if (!original) return;
    void* const top = composite
        ? static_cast<std::byte*>(composite) - kCompositeOffset
        : nullptr;
    if (g_runtime.active && g_runtime.top == top &&
        g_runtime.pending.active && !g_research_explicit_switch) {
        // The frame transaction owns the final route; suppress only the
        // provisional tab callback switch, never ordinary stock navigation.
        g_state_isolation_expectation.consumed = true;
        return;
    }
    if (category != 8) {
        original(composite, category);
        g_research_switch_succeeded = true;
        return;
    }

    const std::size_t logical = g_runtime.pending.active
        ? g_runtime.pending.desired_logical
        : g_runtime.viewport.selected_logical();
    const std::size_t physical = g_runtime.pending.active
        ? g_runtime.pending.desired_physical
        : g_runtime.viewport.selected_physical();
    if (g_research_failed || !g_runtime.active || g_runtime.top != top ||
        !logical_uses_graphics_carrier(logical) ||
        physical >= kPhysicalTabCount || !g_research_explicit_switch ||
        !g_runtime.pending.active) {
        g_research_switch_succeeded = false;
        if (!g_research_failed) {
            g_research_failed = true;
            log_failure(
                "Graphics carrier switch escaped validated pending frame; custom transitions frozen");
        }
        return;
    }

    const std::size_t route = graphics_route_for_logical(logical);
    const std::uint64_t transaction_frame =
        g_research_completed_composite_frames;
    if (!g_research_prepare_carrier(
            composite, route, transaction_frame, &category,
            &switch_research_carrier_isolated)) {
        g_research_failed = true;
        g_research_switch_succeeded = false;
        log_failure(
            "Graphics carrier prepare FAILED CLOSED logical=%zu route=%zu frame=%llu",
            logical, route,
            static_cast<unsigned long long>(transaction_frame));
        return;
    }

    auto* const bytes = static_cast<std::byte*>(composite);
    const PanelSnapshot network = inspect_panel_slot(
        bytes + kCompositePanelCacheOffset + 5 * sizeof(void*));
    const PanelCarrierSnapshot network_carrier =
        inspect_panel_carrier(network);
    const bool switched = category == 8 &&
        switch_research_carrier_isolated(composite, category);
    const PanelSnapshot network_after = inspect_panel_slot(
        bytes + kCompositePanelCacheOffset + 5 * sizeof(void*));
    const PanelCarrierSnapshot network_carrier_after =
        inspect_panel_carrier(network_after);
    const bool network_exact = network.panel == network_after.panel &&
        (!network.panel ||
            (network_carrier.valid && network_carrier_after.valid &&
                network_carrier.wrapper == network_carrier_after.wrapper &&
                network_carrier.payload ==
                    network_carrier_after.payload));
    std::int32_t record_category{-1};
    const bool record_exact = guarded_read(
        record_at(top, physical) + kCategoryIdOffset,
        record_category) && record_category == category;
    if (!switched || !network_exact ||
        transaction_frame != g_research_completed_composite_frames ||
        !g_research_verify_carrier(
            composite, route, category) || !record_exact) {
        g_research_failed = true;
        g_research_switch_succeeded = false;
        log_failure(
            "Graphics carrier activation FAILED CLOSED logical=%zu route=%zu category=%d recordCategory=%d recordWrite=0",
            logical, route, category, record_category);
        return;
    }
    g_runtime.catalogue[logical].category = category;
    g_runtime.pending.desired_category = category;
    g_research_switch_succeeded = true;
    log_line(
        "graphicsCarrier activated logical=%zu route=%zu kind=%s category=8 frame=%llu networkPreserved=1 noInterveningFrame=1 finalCurrentCategory8=1",
        logical, route,
        route == kNativeGraphicsRoute ? "native-graphics" : "custom",
        static_cast<unsigned long long>(transaction_frame));
}
