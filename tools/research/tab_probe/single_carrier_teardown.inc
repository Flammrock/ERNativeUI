void __fastcall tab_content_composite_teardown_detour(void* composite) noexcept {
    const auto original = g_original_tab_content_composite_teardown.load(std::memory_order_acquire);
    if (!original) return;
    ResearchCarrierIdentity category4{};
    if (composite) {
#if defined(_MSC_VER)
        __try {
#endif
            category4 = inspect_research_carrier(
                *reinterpret_cast<void**>(static_cast<std::byte*>(composite) + 0x68 + 4 * 8),
                4, erui::detail::invalid_compiled_index);
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
#endif
    }
    original(composite);
    bool cleared{};
#if defined(_MSC_VER)
    __try {
#endif
        cleared = composite && !*reinterpret_cast<void**>(
            static_cast<std::byte*>(composite) + 0x68 + 4 * 8);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
#endif
    erui::detail::logf(cleared ? erui::LogLevel::info : erui::LogLevel::error,
        "Research single carrier native teardown: epoch=%llu composite=%p category4=%p/%p/%p slot4Null=%d stockGraphicsRetiredByHost=0",
        static_cast<unsigned long long>(g_research_carrier_epoch), composite,
        category4.panel, category4.wrapper, category4.payload, cleared);
    g_research_carriers = {};
    g_reusable_routes = {erui::detail::invalid_compiled_index};
    g_reusable_composite = nullptr;
    g_reusable_activation_frame = 0;
    // Failure is terminal for this process, including across Configuration reopen.
    ++g_research_carrier_epoch;
}
