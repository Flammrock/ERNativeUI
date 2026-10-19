#if defined(ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE)
    if (g_tab_content_category8_pre_hook.enabled()) {
        const auto disabled = g_tab_content_category8_pre_hook.disable();
        if (!disabled) {
            erui::detail::logf(
                erui::LogLevel::warning,
                "Research Graphics-carrier pre-hook could not be disabled cleanly during reset (SafetyHook error type %u)",
                static_cast<unsigned>(disabled.error().type));
        }
    }
    g_tab_content_category8_pre_hook.reset();
    g_tab_content_composite_teardown_hook.reset();
    g_original_tab_content_composite_teardown.store(
        nullptr, std::memory_order_release);
    g_research_carrier = {};
    g_reusable_route = kNativeGraphicsRoute;
    g_reusable_composite = nullptr;
    g_reusable_activation_frame = 0;
    g_reusable_failed = false;
    g_research_carrier_epoch = 0;
    g_tab_content_route_log_count.store(0, std::memory_order_release);
    g_tab_content_route_fault_log_count.store(0, std::memory_order_release);
