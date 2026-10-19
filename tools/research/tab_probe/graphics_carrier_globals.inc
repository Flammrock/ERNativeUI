#if defined(ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE)
SafetyHookMid g_tab_content_category8_pre_hook{};
SafetyHookInline g_tab_content_composite_teardown_hook{};
using ResearchCompositeTeardownFn = void(__fastcall*)(void*);
std::atomic<ResearchCompositeTeardownFn>
    g_original_tab_content_composite_teardown{};
std::atomic_uint32_t g_tab_content_route_log_count{};
std::atomic_uint32_t g_tab_content_route_fault_log_count{};
constexpr std::uintptr_t kCategory8PreRva = 0x93CF89;
constexpr std::uintptr_t kCategory8MaterializerRva = 0x95C050;
constexpr std::uintptr_t kCategory8ExtraCallbackRva = 0x95A820;
constexpr std::uintptr_t kCompositeTeardownRva = 0x93C1A0;
constexpr std::uint8_t kResearchTabCarrierCapacity =
    static_cast<std::uint8_t>(erui::detail::research_tab_main_capacity);
