// Research-only Graphics carrier V2. Category 8 is the sole recyclable
// rows-only carrier. Category 5 is used only as synchronous native parking.
constexpr std::size_t kNativeGraphicsRoute = 50;
constexpr std::size_t kGraphicsRouteCount = 51;
ResearchCarrierIdentity g_research_carrier{};
std::size_t g_reusable_route{kNativeGraphicsRoute};
void* g_reusable_composite{};
std::uint64_t g_reusable_activation_frame{};
bool g_reusable_failed{};
using ResearchParkingSwitchFn = bool(__cdecl*)(void*, std::int32_t);

struct ResearchAllocatorTarget {
    std::uintptr_t vtable;
    std::uintptr_t method;
    std::uint64_t entry;
};

// Exact DLAllocator descendants observed for Configuration panels in the
// local 2.7.1 executable. No unlisted vtable/queue pair is called.
constexpr std::array<ResearchAllocatorTarget, 26>
    kResearchAllocatorTargets{{
        {0x29D54F0,0x2657530,0xFF864788E9CA8B48},
        {0x2B7D418,0xB97EC0,0x74894808245C8948},
        {0x2BDE708,0xE14B60,0x8B4820EC83485340},
        {0x2BDE8F0,0xE156B0,0xC74840EC83485740},
        {0x2BDECA0,0xE16260,0x28C1834808498B48},
        {0x2BDEA48,0xE15BA0,0x28C1834808498B48},
        {0x2BDED78,0xE166D0,0xC74840EC83485740},
        {0x2BDE818,0xE150C0,0xC74840EC83485740},
        {0x2BDEB20,0xE15F30,0x28C1834808498B48},
        {0x2BDEE50,0xE16CA0,0xC74840EC83485740},
        {0x2BE1870,0xE1F250,0x8B4820EC83485340},
        {0x2BE1A20,0xE1F7B0,0x8B4820EC83485340},
        {0x2F26FA8,0x1C59C30,0x00262088E9CA8B48},
        {0x2F273D0,0x1AF6110,0x5FA5EF08900000C2},
        {0x30357D8,0x1B44400,0x28C1834808498B48},
        {0x3051240,0x1C29FE0,0xA33CC7EC900000C2},
        {0x30B0298,0x1EBCE90,0xEB17900000065BE9},
        {0x30C0228,0x1F38D90,0xC74840EC83485740},
        {0x30CA908,0x1F61F50,0x48018B4808498B48},
        {0x30CAE70,0x1F64450,0xC74830EC83485740},
        {0x30CFE80,0x1F81AA0,0x3176CC030E302AE9},
        {0x30CFF58,0x1F81AB0,0x35A19002D12495E9},
        {0x3278EA0,0x26398D0,0xC74830EC83485740},
        {0x30C74A0,0x1A4900,0x28C1834808498B48},
        {0x2BBFF38,0x1538B0,0xC74840EC83485740},
        {0x2BE0F30,0x1F1A8D0,0x48018B4808498B48},
    }};

bool reusable_fault(const char* reason) noexcept {
    g_reusable_failed = true;
    erui::detail::logf(erui::LogLevel::error,
        "Research Graphics carrier V2 FAILED CLOSED: %s; custom transitions frozen, native input remains live",
        reason);
    return false;
}

bool exact_allocator_target(
    std::uintptr_t image,
    void* manager_vtable,
    void* queue) noexcept {
    for (const auto& target : kResearchAllocatorTargets) {
        if (manager_vtable == reinterpret_cast<void*>(
                image + target.vtable) &&
            queue == reinterpret_cast<void*>(image + target.method) &&
            std::memcmp(queue, &target.entry, sizeof(target.entry)) == 0) {
            return true;
        }
    }
    return false;
}

bool cached_identity_is_globally_unique(
    void** slots,
    std::size_t owner_slot,
    const ResearchCarrierIdentity& owner,
    const char* label) noexcept {
    if (!slots || owner_slot >= 10 || !owner.valid ||
        slots[owner_slot] != owner.panel || !owner.wrapper ||
        !owner.payload) {
        erui::detail::logf(erui::LogLevel::error,
            "Research Graphics carrier identity audit rejected %s owner slot=%zu panel=%p wrapper=%p payload=%p valid=%d",
            label ? label : "unknown", owner_slot, owner.panel,
            owner.wrapper, owner.payload, owner.valid ? 1 : 0);
        return false;
    }
    for (std::size_t slot = 0; slot < 10; ++slot) {
        if (slot == owner_slot || !slots[slot]) continue;
        const auto other = inspect_research_carrier(
            slots[slot], static_cast<std::uint8_t>(slot),
            erui::detail::invalid_compiled_index);
        const bool distinct = other.valid &&
            other.panel != owner.panel &&
            other.wrapper != owner.wrapper &&
            other.payload != owner.payload;
        if (!distinct) {
            erui::detail::logf(erui::LogLevel::error,
                "Research Graphics carrier identity audit rejected %s ownerSlot=%zu owner=%p/%p/%p conflictingSlot=%zu other=%p/%p/%p valid=%d",
                label ? label : "unknown", owner_slot,
                owner.panel, owner.wrapper, owner.payload, slot,
                other.panel, other.wrapper, other.payload,
                other.valid ? 1 : 0);
            return false;
        }
    }
    return true;
}

void clear_retired_page_references(void* panel) noexcept {
    if (!panel) return;
    void* expected = panel;
    const bool dialog_cleared = g_dialog_input_page.compare_exchange_strong(
        expected, nullptr, std::memory_order_acq_rel,
        std::memory_order_acquire);
    expected = panel;
    const bool root_cleared = g_last_page.compare_exchange_strong(
        expected, nullptr, std::memory_order_acq_rel,
        std::memory_order_acquire);

    // A tab transition cannot legitimately overlap a pending subpage open.
    // Cancel the request before destroying its possible parent page. Existing
    // child bindings are left intact and are normally removed by native Back.
    const bool pending_subpage =
        g_submenu_navigation.pending_route().valid();
    if (pending_subpage) g_submenu_navigation.cancel_request();
    reset_color_picker_page_widgets(
        panel, ColorPickerWidgetHost::subpage);
    erui::detail::logf(erui::LogLevel::trace,
        "Research Graphics carrier cleared retiring page references panel=%p dialog=%d root=%d pendingSubpageCancelled=%d",
        panel, dialog_cleared ? 1 : 0, root_cleared ? 1 : 0,
        pending_subpage ? 1 : 0);
}

extern "C" __declspec(dllexport) std::uint32_t __cdecl
ERUI_ResearchGraphicsCarrierVersionV2() noexcept {
    return 2;
}

extern "C" __declspec(dllexport) bool __cdecl
ERUI_ResearchPrepareGraphicsCarrierV2(
    void* composite,
    std::size_t route,
    std::uint64_t completed_frame,
    std::int32_t* category,
    ResearchParkingSwitchFn park_native) noexcept {
    if (g_reusable_failed) return false;
    const auto& runtime = erui::detail::runtime_state();
    if (!composite || !category || !park_native || !runtime.menu ||
        runtime.menu->tab_page_indices.size() != 50 ||
        route >= kGraphicsRouteCount ||
        (g_reusable_composite && composite != g_reusable_composite)) {
        return reusable_fault(
            "invalid composite, catalogue, route or output");
    }
    const auto image = reinterpret_cast<std::uintptr_t>(
        g_addresses.game_image_base);
    if (!image || g_addresses.game_image_size != 0x5E0DA00) {
        return reusable_fault(
            "retirement requires exact Elden Ring 2.7.1 image");
    }

#if defined(_MSC_VER)
    __try {
#endif
        constexpr std::array<std::uint8_t, 16> manager_entry{
            0x4C,0x63,0x05,0x81,0xF3,0xF6,0x02,0x33,
            0xD2,0x4D,0x85,0xC0,0x7E,0x1E,0x48,0x8D};
        constexpr std::array<std::uint8_t, 16> deactivate_entry{
            0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,
            0xEC,0x20,0x8B,0xDA,0x48,0x8B,0xF9,0xE8};
        if (std::memcmp(reinterpret_cast<const void*>(image + 0xE1C080),
                manager_entry.data(), manager_entry.size()) != 0 ||
            std::memcmp(reinterpret_cast<const void*>(image + 0x953520),
                deactivate_entry.data(), deactivate_entry.size()) != 0) {
            return reusable_fault(
                "retirement manager/deactivate bytes differed");
        }

        auto* const bytes = static_cast<std::byte*>(composite);
        if (*reinterpret_cast<void**>(bytes) !=
                reinterpret_cast<void*>(image + 0x2B0C950) ||
            *reinterpret_cast<void**>(bytes - 0x1768) !=
                reinterpret_cast<void*>(image + 0x2B16B48)) {
            return reusable_fault(
                "composite/top vtable differed from validated objects");
        }

        auto** const slots = reinterpret_cast<void**>(bytes + 0x68);
        void* current = *reinterpret_cast<void**>(bytes + 0xB8);
        g_reusable_composite = composite;
        constexpr std::int32_t target = 8;
        constexpr std::int32_t parking = 5;

        if (g_reusable_route == route && slots[target]) {
            const auto observed = inspect_research_carrier(
                slots[target], target,
                g_research_carrier.logical_page_index);
            if (!g_research_carrier.committed || !observed.valid ||
                slots[target] != g_research_carrier.panel ||
                observed.wrapper != g_research_carrier.wrapper ||
                observed.payload != g_research_carrier.payload ||
                !cached_identity_is_globally_unique(
                    slots, target, observed,
                    "same-route Graphics carrier")) {
                return reusable_fault(
                    "cached route lost its exact committed Graphics carrier");
            }
            *category = target;
            g_reusable_activation_frame = completed_frame;
            return true;
        }

        void* const old_panel = slots[target];
        if (old_panel) {
            if (completed_frame <= g_reusable_activation_frame) {
                return reusable_fault(
                    "Graphics carrier retirement requires one completed frame");
            }
            const auto observed = inspect_research_carrier(
                old_panel, target,
                g_research_carrier.logical_page_index);
            const auto panel = inspect_builtin_panel(old_panel);
            if (!g_research_carrier.committed || !observed.valid ||
                !panel.valid || g_reusable_route >= kGraphicsRouteCount ||
                observed.panel != g_research_carrier.panel ||
                observed.wrapper != g_research_carrier.wrapper ||
                observed.payload != g_research_carrier.payload ||
                !cached_identity_is_globally_unique(
                    slots, target, observed,
                    "pre-retirement Graphics carrier")) {
                return reusable_fault(
                    "Graphics identity failed before native retirement");
            }

            using ManagerFn = void*(__fastcall*)(void*);
            using DeactivateFn = void(__fastcall*)(void*, bool);
            using QueueFn = void(__fastcall*)(void*, void*);
            void* const manager =
                reinterpret_cast<ManagerFn>(image + 0xE1C080)(old_panel);
            if (!manager) return reusable_fault("retirement manager was null");
            void* const deactivate = *reinterpret_cast<void**>(
                static_cast<std::byte*>(panel.vtable) + 0x08);
            void* const manager_vtable = *reinterpret_cast<void**>(manager);
            void* const queue = manager_vtable
                ? *reinterpret_cast<void**>(
                      static_cast<std::byte*>(manager_vtable) + 0x68)
                : nullptr;
            if (deactivate != reinterpret_cast<void*>(image + 0x953520) ||
                !manager_vtable || !queue ||
                !exact_allocator_target(image, manager_vtable, queue)) {
                return reusable_fault(
                    "retirement allocator/deactivate target was not exact");
            }

            if (old_panel == current) {
                void* const parking_before = slots[parking];
                const auto parking_identity_before = inspect_research_carrier(
                    parking_before, parking,
                    erui::detail::invalid_compiled_index);
                if (parking_before &&
                    (!inspect_builtin_panel(parking_before).valid ||
                        !parking_identity_before.valid ||
                        parking_before == old_panel ||
                        parking_identity_before.wrapper == observed.wrapper ||
                        parking_identity_before.payload == observed.payload ||
                        !cached_identity_is_globally_unique(
                            slots, parking, parking_identity_before,
                            "cached Network parking panel"))) {
                    return reusable_fault(
                        "cached Network parking identity was invalid or aliased");
                }
                if (!park_native(composite, parking)) {
                    return reusable_fault(
                        "native Network parking switch/state isolation failed");
                }
                void* const parked_panel = slots[parking];
                const auto parked = inspect_research_carrier(
                    parked_panel, parking,
                    erui::detail::invalid_compiled_index);
                const auto old_after = inspect_research_carrier(
                    old_panel, target,
                    g_research_carrier.logical_page_index);
                if (!parked_panel || !parked.valid ||
                    !inspect_builtin_panel(parked_panel).valid ||
                    *reinterpret_cast<void**>(bytes + 0xB8) != parked_panel ||
                    parked_panel == old_panel ||
                    parked.wrapper == observed.wrapper ||
                    parked.payload == observed.payload ||
                    slots[target] != old_panel || !old_after.valid ||
                    old_after.wrapper != observed.wrapper ||
                    old_after.payload != observed.payload ||
                    !cached_identity_is_globally_unique(
                        slots, target, old_after,
                        "parked Graphics carrier") ||
                    !cached_identity_is_globally_unique(
                        slots, parking, parked,
                        "active Network parking panel") ||
                    (parking_before &&
                        (parked_panel != parking_before ||
                            parked.wrapper != parking_identity_before.wrapper ||
                            parked.payload != parking_identity_before.payload))) {
                    return reusable_fault(
                        "Network parking did not leave Graphics intact/inactive");
                }
                current = parked_panel;
                erui::detail::logf(erui::LogLevel::info,
                    "Research Graphics carrier parked frame=%llu route=%zu category=5 panel=%p wrapper=%p payload=%p cached=%d currentExact=1 graphicsInactive=1 stateCopySuppressed=1 synchronous=1",
                    static_cast<unsigned long long>(completed_frame), route,
                    parked_panel, parked.wrapper, parked.payload,
                    parking_before != nullptr);
            }

            if (old_panel == *reinterpret_cast<void**>(bytes + 0xB8) ||
                *reinterpret_cast<void**>(bytes + 0xB8) != current) {
                return reusable_fault(
                    "refused retirement because Graphics was current/current changed");
            }
            void* const parking_pre_retire = slots[parking];
            const auto parking_pre_identity = inspect_research_carrier(
                parking_pre_retire, parking,
                erui::detail::invalid_compiled_index);
            if (parking_pre_retire &&
                (!parking_pre_identity.valid ||
                    !cached_identity_is_globally_unique(
                        slots, parking, parking_pre_identity,
                        "pre-retirement Network parking panel"))) {
                return reusable_fault(
                    "Network parking identity was not globally unique before retirement");
            }
            clear_retired_page_references(old_panel);
            reinterpret_cast<DeactivateFn>(deactivate)(old_panel, false);
            if (*reinterpret_cast<void**>(bytes + 0xB8) != current ||
                slots[target] != old_panel) {
                return reusable_fault(
                    "current/cache changed during Graphics deactivation");
            }
            bool queue_returned{};
#if defined(_MSC_VER)
            __try {
#endif
                reinterpret_cast<QueueFn>(queue)(manager, old_panel);
                queue_returned = true;
#if defined(_MSC_VER)
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                // Ownership may already have transferred. Clear the sole
                // cache owner before failing so native teardown cannot queue
                // the same panel a second time.
                slots[target] = nullptr;
            }
#endif
            const bool current_unchanged =
                *reinterpret_cast<void**>(bytes + 0xB8) == current;
            const bool graphics_pointer_unchanged =
                slots[target] == old_panel;
            const bool network_pointer_unchanged =
                slots[parking] == parking_pre_retire;
            // From this point the allocator owns old_panel. Never inspect or
            // dereference it again through slot 8.
            slots[target] = nullptr;
            g_research_carrier = {};
            ++g_research_carrier_epoch;
            if (g_research_carrier_epoch == 0) {
                g_research_carrier_epoch = 1;
            }
            if (!queue_returned || !current_unchanged ||
                !graphics_pointer_unchanged ||
                !network_pointer_unchanged) {
                return reusable_fault(
                    "post-queue raw cache/current invariants differed; slot 8 was cleared to prevent double retirement");
            }
            erui::detail::logf(erui::LogLevel::info,
                "Research Graphics carrier retired frame=%llu activationFrame=%llu ageFrames=%llu priorRoute=%zu nextRoute=%zu old=%p/%p/%p active=%p manager=%p managerVtable=%p deactivate=%p queue=%p allocatorExact=1 slot8Null=%d currentUntouched=%d networkUntouched=1",
                static_cast<unsigned long long>(completed_frame),
                static_cast<unsigned long long>(g_reusable_activation_frame),
                static_cast<unsigned long long>(
                    completed_frame - g_reusable_activation_frame),
                g_reusable_route, route, old_panel, observed.wrapper,
                observed.payload, current, manager, manager_vtable,
                deactivate, queue, slots[target] == nullptr,
                *reinterpret_cast<void**>(bytes + 0xB8) == current);
        } else if (g_research_carrier.committed) {
            return reusable_fault(
                "empty Graphics cache retained committed identity");
        }

        g_reusable_route = route;
        g_reusable_activation_frame = completed_frame;
        *category = target;
        erui::detail::logf(erui::LogLevel::info,
            "Research Graphics carrier prepared frame=%llu route=%zu kind=%s current=%p slot8Empty=%d",
            static_cast<unsigned long long>(completed_frame), route,
            route == kNativeGraphicsRoute ? "native-graphics" : "custom",
            current, slots[target] == nullptr);
        return true;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return reusable_fault(
            "SEH during Graphics staging/retirement");
    }
#endif
}

extern "C" __declspec(dllexport) bool __cdecl
ERUI_ResearchVerifyGraphicsCarrierV2(
    void* composite,
    std::size_t route,
    std::int32_t category) noexcept {
    if (g_reusable_failed || composite != g_reusable_composite ||
        route >= kGraphicsRouteCount || category != 8) return false;
#if defined(_MSC_VER)
    __try {
#endif
        const auto& identity = g_research_carrier;
        const auto observed = inspect_research_carrier(
            identity.panel, 8, identity.logical_page_index);
        const auto panel = inspect_builtin_panel(identity.panel);
        auto* const bytes = static_cast<std::byte*>(composite);
        const bool exact = identity.committed &&
            g_reusable_route == route && observed.valid && panel.valid &&
            observed.wrapper == identity.wrapper &&
            observed.payload == identity.payload &&
            *reinterpret_cast<void**>(bytes + 0xB8) == identity.panel &&
            *reinterpret_cast<void**>(bytes + 0x68 + 8 * 8) ==
                identity.panel &&
            cached_identity_is_globally_unique(
                reinterpret_cast<void**>(bytes + 0x68), 8, observed,
                "activated Graphics carrier");
        erui::detail::logf(
            exact ? erui::LogLevel::info : erui::LogLevel::error,
            "Research Graphics carrier activated route=%zu kind=%s category=8 panel=%p wrapper=%p payload=%p rows=%llu committed=%d currentExact=%d",
            route,
            route == kNativeGraphicsRoute ? "native-graphics" : "custom",
            identity.panel, identity.wrapper, identity.payload,
            static_cast<unsigned long long>(panel.native_count),
            identity.committed, exact);
        return exact || reusable_fault(
            "activation did not commit requested Graphics route");
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return reusable_fault("SEH during Graphics activation verification");
    }
#endif
}
