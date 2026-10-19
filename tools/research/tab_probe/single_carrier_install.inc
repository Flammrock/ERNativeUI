bool install_tab_content_research_hooks(const GameAddresses& addresses) noexcept {
    constexpr std::array<std::uint8_t, 7> callback{
        0x48, 0x8D, 0x05, 0x2E, 0x07, 0x02, 0x00};
    constexpr std::array<std::uint8_t, 13> builder{
        0x48, 0x8D, 0x8C, 0x24, 0x80, 0x00, 0x00, 0x00,
        0xE8, 0x12, 0xB3, 0xEC, 0xFF};
    constexpr std::array<std::uint8_t, 23> route{
        0xC6, 0x44, 0x24, 0x20, 0x06,
        0x44, 0x0F, 0xB6, 0xCF, 0x4C, 0x8B, 0xC3,
        0x48, 0x8B, 0xD0, 0x48, 0x8B, 0xCE,
        0xE8, 0xFC, 0x31, 0x02, 0x00};
    constexpr std::array<std::uint8_t, 16> teardown_entry{
        0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x56, 0x57,
        0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x30};
    if (!addresses.game_image_base || addresses.game_image_size != 0x5E0DA00 ||
        !tab_content_bytes_match(addresses.game_image_base + 0x93CACB, callback) ||
        !tab_content_bytes_match(addresses.game_image_base + 0x93CAF1, builder) ||
        !tab_content_bytes_match(addresses.game_image_base + 0x93CB1D, route) ||
        !tab_content_bytes_match(addresses.game_image_base + kCompositeTeardownRva, teardown_entry)) {
        erui::detail::logf(erui::LogLevel::error,
            "Research single category-4 selector-6 route signatures differed; no hooks installed");
        return false;
    }
    auto pre4 = SafetyHookMid::create(addresses.game_image_base + kCategory4PreRva,
        &begin_tab_content_category4_route, SafetyHookMid::StartDisabled);
    if (!pre4) return false;
    g_tab_content_category4_pre_hook = std::move(*pre4);
    auto teardown = SafetyHookInline::create(addresses.game_image_base + kCompositeTeardownRva,
        reinterpret_cast<void*>(&tab_content_composite_teardown_detour), SafetyHookInline::StartDisabled);
    if (!teardown) {
        g_tab_content_category4_pre_hook.reset();
        return false;
    }
    g_tab_content_composite_teardown_hook = std::move(*teardown);
    g_original_tab_content_composite_teardown.store(
        g_tab_content_composite_teardown_hook.original<ResearchCompositeTeardownFn>(), std::memory_order_release);
    if (!g_tab_content_composite_teardown_hook.enable() || !g_tab_content_category4_pre_hook.enable()) {
        g_tab_content_category4_pre_hook.reset();
        g_tab_content_composite_teardown_hook.reset();
        g_original_tab_content_composite_teardown.store(nullptr, std::memory_order_release);
        return false;
    }
    erui::detail::logf(erui::LogLevel::info,
        "Research single carrier installed category=4 selector=6 pre=+0x%llX native=+0x%llX thunk=%p teardown=+0x%llX parking=stock-category8 synchronous=1 stockGraphicsMutation=0",
        static_cast<unsigned long long>(kCategory4PreRva),
        static_cast<unsigned long long>(kCategory4MaterializerRva),
        reinterpret_cast<void*>(&tab_content_category4_materializer),
        static_cast<unsigned long long>(kCompositeTeardownRva));
    return true;
}
