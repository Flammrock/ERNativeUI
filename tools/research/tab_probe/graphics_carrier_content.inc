#if defined(ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE)
void __fastcall tab_content_category8_materializer(
    void* page,
    void* menu_option_data) noexcept;

constexpr std::size_t kPanelSceneProxyOffset = 0x1200;
constexpr std::size_t kSceneProxyWrapperFlagsOffset = 0x20;
constexpr std::size_t kSceneProxyWrapperPayloadOffset = 0x28;
constexpr std::uintptr_t kSceneObjProxyVtableRva = 0x2A97AF0;
constexpr std::uintptr_t kSceneObjProxyGetValueRva = 0x74C930;

struct ResearchCarrierIdentity {
    void* panel{};
    void* wrapper{};
    void* payload{};
    std::uint32_t wrapper_flags{};
    std::size_t logical_page_index{
        erui::detail::invalid_compiled_index};
    std::uint8_t category{};
    bool valid{};
    bool committed{};
};

std::uint64_t g_research_carrier_epoch{};

ResearchCarrierIdentity inspect_research_carrier(
    void* page,
    std::uint8_t category,
    std::size_t logical_page_index) noexcept {
    ResearchCarrierIdentity result{};
    result.panel = page;
    result.category = category;
    result.logical_page_index = logical_page_index;
    if (!page || !g_addresses.game_image_base ||
        kSceneObjProxyGetValueRva >= g_addresses.game_image_size ||
        kSceneObjProxyVtableRva >= g_addresses.game_image_size) {
        return result;
    }
#if defined(_MSC_VER)
    __try {
#endif
        const std::uintptr_t image = reinterpret_cast<std::uintptr_t>(
            g_addresses.game_image_base);
        auto* const proxy = static_cast<std::byte*>(page) +
            kPanelSceneProxyOffset;
        void* const proxy_vtable = *reinterpret_cast<void**>(proxy);
        void* const expected_vtable = reinterpret_cast<void*>(
            image + kSceneObjProxyVtableRva);
        void* const getter = proxy_vtable
            ? *reinterpret_cast<void**>(
                  static_cast<std::byte*>(proxy_vtable) + sizeof(void*))
            : nullptr;
        if (proxy_vtable != expected_vtable ||
            getter != reinterpret_cast<void*>(
                image + kSceneObjProxyGetValueRva)) {
            return result;
        }
        using GetterFn = void*(__fastcall*)(void*);
        result.wrapper = reinterpret_cast<GetterFn>(getter)(proxy);
        if (!result.wrapper) return result;
        result.wrapper_flags = *reinterpret_cast<const std::uint32_t*>(
            static_cast<const std::byte*>(result.wrapper) +
                kSceneProxyWrapperFlagsOffset);
        result.payload = *reinterpret_cast<void* const*>(
            static_cast<const std::byte*>(result.wrapper) +
                kSceneProxyWrapperPayloadOffset);
        result.valid = result.payload != nullptr &&
            (result.wrapper_flags & 0x8Fu) >= 2u;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = {};
        result.panel = page;
        result.category = category;
        result.logical_page_index = logical_page_index;
    }
#endif
    return result;
}

#include "reusable_carrier_host.inc"

void log_tab_content_route_fault(const char* message) noexcept {
    const std::uint32_t event =
        g_tab_content_route_fault_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (event <= 8) {
        erui::detail::logf(erui::LogLevel::error,
            "Research Graphics-carrier route fault[%u]: %s",
            static_cast<unsigned>(event), message);
    }
}

void begin_tab_content_category8_route(
    safetyhook::Context& context) noexcept {
    const std::uintptr_t image = reinterpret_cast<std::uintptr_t>(
        g_addresses.game_image_base);
    if (!image || kCategory8MaterializerRva >=
            g_addresses.game_image_size) {
        log_tab_content_route_fault("invalid image bounds");
        return;
    }
    bool replaced{};
    bool secondary_zeroed{};
    std::uint8_t observed_selector{};
    std::uintptr_t observed_primary_slot{};
    std::uintptr_t observed_secondary_slot{};
    std::uint8_t observed_enabled{};
    std::uint8_t observed_reserved{};
    std::uintptr_t observed_primary_callback{};
    std::uint32_t observed_primary_metadata{};
    std::uintptr_t observed_secondary_callback{};
    std::uint32_t observed_secondary_metadata{};
#if defined(_MSC_VER)
    __try {
#endif
        auto* const stack = reinterpret_cast<std::byte*>(context.rsp);
        observed_selector =
            *reinterpret_cast<const std::uint8_t*>(stack + 0x20);
        observed_primary_slot =
            *reinterpret_cast<const std::uintptr_t*>(stack + 0x28);
        observed_secondary_slot =
            *reinterpret_cast<const std::uintptr_t*>(stack + 0x30);
        observed_enabled =
            *reinterpret_cast<const std::uint8_t*>(stack + 0x38);
        observed_reserved =
            *reinterpret_cast<const std::uint8_t*>(stack + 0x40);
        auto* const secondary_callback =
            reinterpret_cast<std::uintptr_t*>(
            stack + 0x50);
        auto* const secondary_metadata =
            reinterpret_cast<std::uint32_t*>(stack + 0x58);
        auto* const primary_callback =
            reinterpret_cast<std::uintptr_t*>(
            stack + 0x60);
        auto* const primary_metadata =
            reinterpret_cast<std::uint32_t*>(stack + 0x68);
        observed_primary_callback = *primary_callback;
        observed_primary_metadata = *primary_metadata;
        observed_secondary_callback = *secondary_callback;
        observed_secondary_metadata = *secondary_metadata;
        // Both callback descriptors occupy 16 stack bytes, but the wrapper
        // initializes only an eight-byte callback and four-byte metadata
        // field. The final four bytes are padding and must neither be read as
        // metadata nor mutated.
        const bool exact = observed_selector == 11 &&
            observed_primary_slot == context.rsp + 0x60 &&
            observed_secondary_slot == context.rsp + 0x50 &&
            observed_enabled == 1 && observed_reserved == 0 &&
            observed_primary_callback ==
                image + kCategory8MaterializerRva &&
            observed_primary_metadata == 0 &&
            observed_secondary_callback ==
                image + kCategory8ExtraCallbackRva &&
            observed_secondary_metadata == 0;
        if (exact) {
            *primary_callback = reinterpret_cast<std::uintptr_t>(
                &tab_content_category8_materializer);
            if (g_reusable_route != kNativeGraphicsRoute) {
                // Selector 11 invokes this callback after row materialization.
                // A custom route must use the generic post-init branch.
                *secondary_callback = 0;
                *secondary_metadata = 0;
                secondary_zeroed = true;
            }
            replaced = true;
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_tab_content_route_fault("SEH reading callback locals");
        return;
    }
#endif
    if (!replaced) {
        const std::uint32_t event =
            g_tab_content_route_fault_log_count.fetch_add(
                1, std::memory_order_relaxed) + 1;
        if (event <= 8) {
            erui::detail::logf(erui::LogLevel::error,
                "Research Graphics-carrier route fault[%u]: unexpected selector/callback stack-local contract observed={selector:%u primarySlot:%p secondarySlot:%p enabled:%u reserved:%u primary:%p/0x%08X secondary:%p/0x%08X}",
                static_cast<unsigned>(event),
                static_cast<unsigned>(observed_selector),
                reinterpret_cast<void*>(observed_primary_slot),
                reinterpret_cast<void*>(observed_secondary_slot),
                static_cast<unsigned>(observed_enabled),
                static_cast<unsigned>(observed_reserved),
                reinterpret_cast<void*>(observed_primary_callback),
                static_cast<unsigned>(observed_primary_metadata),
                reinterpret_cast<void*>(observed_secondary_callback),
                static_cast<unsigned>(observed_secondary_metadata));
        }
        return;
    }
    const std::uint32_t event =
        g_tab_content_route_log_count.fetch_add(
            1, std::memory_order_relaxed) + 1;
    if (event <= 32) {
        erui::detail::logf(erui::LogLevel::trace,
            "Research Graphics route begin[%u]: route=%zu kind=%s selector=11 primary=host-thunk secondary=%s",
            static_cast<unsigned>(event), g_reusable_route,
            g_reusable_route == kNativeGraphicsRoute
                ? "native-graphics" : "custom",
            secondary_zeroed ? "zero/generic-post-init" :
                "native-0x95A820");
    }
}

void materialize_native_graphics(void* page) noexcept {
    const std::uintptr_t image = reinterpret_cast<std::uintptr_t>(
        g_addresses.game_image_base);
    if (!page || !image || kCategory8MaterializerRva >=
            g_addresses.game_image_size) return;
    reinterpret_cast<BuiltinPanelMaterializerFn>(
        image + kCategory8MaterializerRva)(
            page, static_cast<std::byte*>(page) + 0x1B38);
}

void __fastcall tab_content_category8_materializer(
    void* page,
    void* untrusted_argument2) noexcept {
    (void)untrusted_argument2;
    if (!page || g_reusable_failed || g_research_carrier.committed) {
        reusable_fault(
            "materialization occurred without exact prior Graphics retirement");
        return;
    }
    if (g_research_carrier_epoch == 0) g_research_carrier_epoch = 1;

    const BuiltinPanelSnapshot before = inspect_builtin_panel(page);
    const ResearchCarrierIdentity initial = inspect_research_carrier(
        page, 8, erui::detail::invalid_compiled_index);
    if (!before.valid || before.native_count != 0 || !initial.valid) {
        reusable_fault("fresh Graphics panel failed pre-mutation identity");
        return;
    }

    if (g_reusable_route == kNativeGraphicsRoute) {
        materialize_native_graphics(page);
        const BuiltinPanelSnapshot after = inspect_builtin_panel(page);
        ResearchCarrierIdentity identity = inspect_research_carrier(
            page, 8, erui::detail::invalid_compiled_index);
        const bool committed = after.valid && after.native_count > 0 &&
            after.native_count <= after.visual_capacity && identity.valid &&
            identity.wrapper == initial.wrapper &&
            identity.payload == initial.payload;
        if (committed) {
            identity.committed = true;
            g_research_carrier = identity;
        }
        erui::detail::logf(
            committed ? erui::LogLevel::info : erui::LogLevel::error,
            "Research Graphics carrier construct: epoch=%llu route=%zu kind=native-graphics page=%p wrapper=%p payload=%p before=%llu after=%llu visualRows=%u committed=%d extraCallback=native",
            static_cast<unsigned long long>(g_research_carrier_epoch),
            g_reusable_route,
            page, identity.wrapper, identity.payload,
            static_cast<unsigned long long>(before.native_count),
            static_cast<unsigned long long>(after.native_count),
            static_cast<unsigned>(after.visual_capacity),
            committed ? 1 : 0);
        if (!committed) reusable_fault("native Graphics restoration failed");
        return;
    }

    erui::detail::RuntimeState& runtime = erui::detail::runtime_state();
    if (!runtime.options.enable_row_injection || !runtime.menu ||
        runtime.menu->tab_page_indices.size() != 50 ||
        g_reusable_route >= runtime.menu->tab_page_indices.size()) {
        reusable_fault("custom Graphics route had no exact compiled fixture");
        return;
    }
    const std::size_t logical_page_index =
        runtime.menu->tab_page_indices[g_reusable_route];
    if (logical_page_index >= runtime.menu->pages.size() ||
        runtime.menu->pages[logical_page_index].kind != erui::PageKind::tab) {
        reusable_fault("custom Graphics route resolved invalid tab page");
        return;
    }
    const std::size_t logical_rows =
        runtime.menu->pages[logical_page_index].rows.size();
    if (logical_rows != 20) {
        reusable_fault("custom Graphics fixture no longer has twenty logical rows");
        return;
    }

    g_dialog_input_page.store(page, std::memory_order_release);
    const erui::detail::PageRoute route =
        erui::detail::PageRoute::tab_main(
            logical_page_index,
            static_cast<std::uint8_t>(before.visual_capacity));
    erui::detail::PageSlice expected_slice{};
    if (!resolve_page_route_slice(*runtime.menu, route, expected_slice)) {
        reusable_fault("custom Graphics route slice could not resolve");
        return;
    }
    const std::size_t expected_native_rows = expected_slice.row_count +
        (expected_slice.has_previous ? 1u : 0u) +
        (expected_slice.has_next ? 1u : 0u);
    if (expected_native_rows == 0 ||
        expected_native_rows > before.visual_capacity) {
        reusable_fault("custom rows exceeded Graphics capacity");
        return;
    }
    const RowInjectionOutcome outcome = inject_page_route(
        page, route, g_addresses);
    const BuiltinPanelSnapshot after = inspect_builtin_panel(page);
    ResearchCarrierIdentity identity = inspect_research_carrier(
        page, 8, logical_page_index);
    const bool identity_stable = identity.valid &&
        identity.wrapper == initial.wrapper &&
        identity.payload == initial.payload;
    const bool committed = after.valid && outcome.faulted == 0 &&
        outcome.skipped == 0 && outcome.attempted == expected_native_rows &&
        outcome.added == expected_native_rows &&
        after.native_count == expected_native_rows &&
        after.native_count <= after.visual_capacity && identity_stable;
    if (committed) {
        identity.committed = true;
        g_research_carrier = identity;
    }
    erui::detail::logf(
        committed ? erui::LogLevel::info : erui::LogLevel::error,
        "Research Graphics carrier construct: epoch=%llu route=%zu kind=custom page=%p wrapper=%p payload=%p logicalPage=%zu before=%llu after=%llu visualRows=%u logicalRows=%zu attempted=%u added=%u faulted=%u skipped=%u identityStable=%d committed=%d extraCallback=zero/generic-post-init",
        static_cast<unsigned long long>(g_research_carrier_epoch),
        g_reusable_route, page, identity.wrapper, identity.payload,
        logical_page_index,
        static_cast<unsigned long long>(before.native_count),
        static_cast<unsigned long long>(after.native_count),
        static_cast<unsigned>(before.visual_capacity), logical_rows,
        static_cast<unsigned>(outcome.attempted),
        static_cast<unsigned>(outcome.added),
        static_cast<unsigned>(outcome.faulted),
        static_cast<unsigned>(outcome.skipped),
        identity_stable ? 1 : 0, committed ? 1 : 0);
    if (!committed) reusable_fault("custom Graphics materialization failed");
}

void __fastcall tab_content_composite_teardown_detour(
    void* composite) noexcept {
    const auto original = g_original_tab_content_composite_teardown.load(
        std::memory_order_acquire);
    if (!original) return;
    ResearchCarrierIdentity category8{};
    bool slot_read{};
    if (composite) {
#if defined(_MSC_VER)
        __try {
#endif
            void* const panel = *reinterpret_cast<void**>(
                static_cast<std::byte*>(composite) + 0x68 + 8 * 8);
            category8 = inspect_research_carrier(
                panel,
                8, g_research_carrier.logical_page_index);
            slot_read = true;
#if defined(_MSC_VER)
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
#endif
    }
    const bool tracked = g_research_carrier.committed;
    const bool exact_tracked = tracked && slot_read && category8.valid &&
        composite == g_reusable_composite &&
        category8.panel == g_research_carrier.panel &&
        category8.wrapper == g_research_carrier.wrapper &&
        category8.payload == g_research_carrier.payload;
    const bool clean_empty = !tracked && slot_read && !category8.panel &&
        (!g_reusable_composite || composite == g_reusable_composite);
    if (category8.panel) {
        clear_retired_page_references(category8.panel);
    }
    original(composite);
    bool cleared{};
#if defined(_MSC_VER)
    __try {
#endif
        cleared = composite && !*reinterpret_cast<void**>(
            static_cast<std::byte*>(composite) + 0x68 + 8 * 8);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
#endif
    const bool exact_teardown = cleared &&
        (exact_tracked || clean_empty);
    erui::detail::logf(
        exact_teardown ? erui::LogLevel::info : erui::LogLevel::error,
        "Research Graphics carrier native teardown: epoch=%llu composite=%p route=%zu category8=%p/%p/%p tracked=%d exactTracked=%d cleanEmpty=%d slot8Null=%d networkRetiredByHost=0",
        static_cast<unsigned long long>(g_research_carrier_epoch),
        composite, g_reusable_route, category8.panel, category8.wrapper,
        category8.payload, tracked ? 1 : 0, exact_tracked ? 1 : 0,
        clean_empty ? 1 : 0, cleared ? 1 : 0);
    if (exact_teardown) {
        g_research_carrier = {};
        g_reusable_route = kNativeGraphicsRoute;
        g_reusable_composite = nullptr;
        g_reusable_activation_frame = 0;
        ++g_research_carrier_epoch;
        if (g_research_carrier_epoch == 0) {
            g_research_carrier_epoch = 1;
        }
    } else {
        (void)reusable_fault(
            "native teardown did not match the tracked Graphics identity; retained diagnostics were not reset");
    }
}
