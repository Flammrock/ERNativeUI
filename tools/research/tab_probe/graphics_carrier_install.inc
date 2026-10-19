#if defined(ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE)
template <std::size_t Size>
bool tab_content_bytes_match(
    const std::uint8_t* address,
    const std::array<std::uint8_t, Size>& expected) noexcept {
    if (!address) return false;
#if defined(_MSC_VER)
    __try {
#endif
        return std::memcmp(address, expected.data(), expected.size()) == 0;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

bool install_tab_content_research_hooks(
    const GameAddresses& addresses) noexcept {
    // Category 8 (Graphics), Elden Ring 2.7.1.0. Native Graphics preserves
    // both callbacks. Each 16-byte descriptor contains an eight-byte callback,
    // four-byte metadata field and four bytes of padding. A validated custom
    // route substitutes the callback at stack+0x60 and clears only the
    // authored callback/metadata fields at stack+0x50.
    constexpr std::array<std::uint8_t, 7> extra_callback{
        0x48, 0x8D, 0x05, 0xF9, 0xD8, 0x01, 0x00};
    constexpr std::array<std::uint8_t, 7> row_callback{
        0x48, 0x8D, 0x05, 0x17, 0xF1, 0x01, 0x00};
    constexpr std::array<std::uint8_t, 13> builder{
        0x48, 0x8D, 0x8C, 0x24, 0x80, 0x00, 0x00, 0x00,
        0xE8, 0xCB, 0x04, 0x00, 0x00};
    constexpr std::array<std::uint8_t, 23> route{
        0xC6, 0x44, 0x24, 0x20, 0x0B,
        0x44, 0x0F, 0xB6, 0xCF, 0x4C, 0x8B, 0xC3,
        0x48, 0x8B, 0xD0, 0x48, 0x8B, 0xCE,
        0xE8, 0x95, 0x2D, 0x02, 0x00};
    constexpr std::array<std::uint8_t, 16> teardown_entry{
        0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x56, 0x57,
        0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x30};
    const std::uintptr_t image = reinterpret_cast<std::uintptr_t>(
        addresses.game_image_base);
    if (!image || addresses.game_image_size != 0x5E0DA00 ||
        !tab_content_bytes_match(
            addresses.game_image_base + 0x93CF20, extra_callback) ||
        !tab_content_bytes_match(
            addresses.game_image_base + 0x93CF32, row_callback) ||
        !tab_content_bytes_match(
            addresses.game_image_base + 0x93CF58, builder) ||
        !tab_content_bytes_match(
            addresses.game_image_base + 0x93CF84, route) ||
        !tab_content_bytes_match(
            addresses.game_image_base + kCompositeTeardownRva,
            teardown_entry)) {
        erui::detail::logf(erui::LogLevel::error,
            "Research Graphics-carrier signatures differed; no hooks installed");
        return false;
    }

    auto pre8 = SafetyHookMid::create(
        addresses.game_image_base + kCategory8PreRva,
        &begin_tab_content_category8_route,
        SafetyHookMid::StartDisabled);
    if (!pre8) return false;
    g_tab_content_category8_pre_hook = std::move(*pre8);

    auto teardown = SafetyHookInline::create(
        addresses.game_image_base + kCompositeTeardownRva,
        reinterpret_cast<void*>(&tab_content_composite_teardown_detour),
        SafetyHookInline::StartDisabled);
    if (!teardown) {
        g_tab_content_category8_pre_hook.reset();
        return false;
    }
    g_tab_content_composite_teardown_hook = std::move(*teardown);
    g_original_tab_content_composite_teardown.store(
        g_tab_content_composite_teardown_hook
            .original<ResearchCompositeTeardownFn>(),
        std::memory_order_release);

    if (!g_tab_content_composite_teardown_hook.enable() ||
        !g_tab_content_category8_pre_hook.enable()) {
        g_tab_content_category8_pre_hook.reset();
        g_tab_content_composite_teardown_hook.reset();
        g_original_tab_content_composite_teardown.store(
            nullptr, std::memory_order_release);
        return false;
    }

    erui::detail::logf(erui::LogLevel::info,
        "Research Graphics carrier V2 installed category=8 selector=11 pre=+0x%llX nativeRows=+0x%llX extraCallback=+0x%llX policy={native:preserve custom:validated-zero} teardown=+0x%llX parking=stock-category5 synchronous=1",
        static_cast<unsigned long long>(kCategory8PreRva),
        static_cast<unsigned long long>(kCategory8MaterializerRva),
        static_cast<unsigned long long>(kCategory8ExtraCallbackRva),
        static_cast<unsigned long long>(kCompositeTeardownRva));
    return true;
}
