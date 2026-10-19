# Build-only Graphics-carrier V2 variants. Production translation units remain
# byte-for-byte untouched.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/hooks.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/host_registry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/tab_viewport_probe.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_globals.inc"
    "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_content.inc"
    "${CMAKE_CURRENT_LIST_DIR}/reusable_carrier_host.inc"
    "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_install.inc"
    "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_reset.inc"
    "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_viewport_switch.inc"
    "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_commit.inc")

function(research_replace variable before after)
    string(FIND "${${variable}}" "${before}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Graphics-carrier research patch no longer matches: ${before}")
    endif()
    string(REPLACE "${before}" "${after}" replaced "${${variable}}")
    set(${variable} "${replaced}" PARENT_SCOPE)
endfunction()

function(research_section variable begin end replacement)
    string(FIND "${${variable}}" "${begin}" first)
    string(FIND "${${variable}}" "${begin}" last REVERSE)
    if(first EQUAL -1 OR NOT first EQUAL last)
        message(FATAL_ERROR
            "Research section start is missing or ambiguous: ${begin}")
    endif()
    string(SUBSTRING "${${variable}}" ${first} -1 tail)
    string(FIND "${tail}" "${end}" count)
    if(count EQUAL -1)
        message(FATAL_ERROR "Research section end is missing: ${end}")
    endif()
    string(SUBSTRING "${${variable}}" ${first} ${count} before)
    research_replace(${variable} "${before}" "${replacement}")
    set(${variable} "${${variable}}" PARENT_SCOPE)
endfunction()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/hooks.cpp" research_hooks)
file(READ "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_globals.inc"
    research_globals)
file(READ "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_content.inc"
    research_content)
file(READ "${CMAKE_CURRENT_LIST_DIR}/reusable_carrier_host.inc"
    research_host)
file(READ "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_install.inc"
    research_install)
file(READ "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_reset.inc"
    research_reset)
research_replace(research_content
    "#include \"reusable_carrier_host.inc\"" "${research_host}")

research_section(research_hooks
    "#if defined(ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE)\nSafetyHookMid g_tab_content_category4_pre_hook{};"
    "#endif\nSafetyHookInline g_camera_panel_materializer_hook{};"
    "${research_globals}")
research_section(research_hooks
    "#if defined(ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE)\nvoid __fastcall tab_content_category4_materializer("
    "#endif\n\nvoid inject_builtin_panel_rows("
    "${research_content}\n")
research_section(research_hooks
    "#if defined(ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE)\ntemplate <std::size_t Size>\nbool tab_content_bytes_match("
    "#endif\n\ntemplate <typename Function>"
    "${research_install}\n")
research_section(research_hooks
    "#if defined(ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE)\n    if (g_tab_content_category9_pre_hook.enabled()) {"
    "#endif\n    reset_dialog_input_gates(false);"
    "${research_reset}")
research_replace(research_hooks
    "runtime.menu->tab_page_indices.size() != 2"
    "runtime.menu->tab_page_indices.size() != 50")
research_replace(research_hooks
    "Research tab-content host requires exactly two compiled custom-tab pages and validated category-4/category-9 native carrier routes"
    "Research Graphics-carrier V2 host requires exactly fifty compiled custom-tab pages with twenty rows each and the validated category-8 selector-11 route")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/research-carrier")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/research-carrier/hooks.cpp"
    "${research_hooks}")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/host_registry.cpp"
    research_registry)
research_replace(research_registry "if (copied_tabs != 2)"
    "if (copied_tabs != 50)")
research_replace(research_registry
    "requires exactly two disabled transports"
    "requires exactly fifty disabled transports")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/research-carrier/host_registry.cpp"
    "${research_registry}")

# Generate the companion viewport probe so Graphics-carrier research cannot
# leak into the ordinary research source or production target.
file(READ "${CMAKE_CURRENT_LIST_DIR}/tab_viewport_probe.cpp"
    research_viewport)
file(READ "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_viewport_switch.inc"
    research_viewport_switch)
file(READ "${CMAKE_CURRENT_LIST_DIR}/graphics_carrier_commit.inc"
    research_viewport_commit)

research_replace(research_viewport
    "// Fifty logical pages reuse one category-4 selector-6 carrier. Stock Graphics\n// is synchronous parking while the exact old category-4 panel is retired."
    "// Native Graphics and fifty custom routes sequentially reuse category 8's\n// selector-11 rows-only carrier. Stock Network is synchronous parking.")
research_replace(research_viewport
    "constexpr bool is_research_custom_category(std::int32_t category) noexcept {\n    return category == 4;\n}"
    "constexpr bool is_research_custom_category(std::int32_t category) noexcept {\n    return category == 8;\n}")
research_replace(research_viewport
    "    void* normalized_category4_panel{};\n    void* normalized_category4_payload{};\n"
    "")
research_section(research_viewport
    "bool normalize_category4_brightness_carrier(void* top) noexcept {"
    "void preserve_active_aliased_carrier_visibility("
    "")
research_replace(research_viewport
    "static_cast<void>(normalize_category4_brightness_carrier(top));"
    "static_cast<void>(true) /* Graphics is the rows-only carrier */;")
research_replace(research_viewport
    "using ResearchVerifyCarrierFn = bool(__cdecl*)(void*, std::size_t, std::int32_t);"
    "using ResearchVerifyCarrierFn = bool(__cdecl*)(void*, std::size_t, std::int32_t);\nusing ResearchCarrierVersionFn = std::uint32_t(__cdecl*)();")
research_section(research_viewport
    "// Called synchronously by the host only after its exact identity, age, and\n// native retirement-target validation."
    "void invoke_switch_panel("
    "${research_viewport_switch}")
research_section(research_viewport
    "PendingTransitionCommitResult commit_pending_transition("
    "void __fastcall selection_callback_detour("
    "${research_viewport_commit}")
research_replace(research_viewport
    "GetProcAddress(host, \"ERUI_ResearchPrepareSingleCarrierV1\")"
    "GetProcAddress(host, \"ERUI_ResearchPrepareGraphicsCarrierV2\")")
research_replace(research_viewport
    "GetProcAddress(host, \"ERUI_ResearchVerifySingleCarrierV1\")"
    "GetProcAddress(host, \"ERUI_ResearchVerifyGraphicsCarrierV2\")")
research_replace(research_viewport
    "    if (!g_research_prepare_carrier || !g_research_verify_carrier) {\n        log_line(\"ERROR: reusable carrier research exports missing; no hooks installed\");\n        return false;\n    }"
    "    ResearchCarrierVersionFn const carrier_version =\n        reinterpret_cast<ResearchCarrierVersionFn>(GetProcAddress(\n            host, \"ERUI_ResearchGraphicsCarrierVersionV2\"));\n    if (!g_research_prepare_carrier || !g_research_verify_carrier ||\n        !carrier_version || carrier_version() != 2) {\n        log_line(\"ERROR: exact Graphics carrier V2 exports missing; no hooks installed\");\n        return false;\n    }\n    log_line(\"Validated Graphics carrier research protocol V2\");")
research_replace(research_viewport
    "// Every custom tab resolves through the one category-4 carrier.\n        next.catalogue[logical].category = 4;"
    "// Every custom tab resolves through the category-8 Graphics carrier.\n        next.catalogue[logical].category = 8;")
research_replace(research_viewport
    "    if (g_research_failed) {\n        const std::uint8_t blocked{};\n        original(composite, frame_value, &blocked);\n        return;\n    }"
    "    if (g_research_failed) {\n        // Freeze only tab mutations. Never swallow native Back/System input.\n        original(composite, frame_value, input_enabled);\n        return;\n    }")

research_replace(research_viewport
    "    const bool freeze_mapping_change = change.valid &&\n        (change.mapping_changed || presentation_changed) &&\n        g_runtime.destructive_transition_failed;"
    "    const bool frozen_carrier_change = change.valid &&\n        change.logical_selection_changed && g_research_failed &&\n        (logical_uses_graphics_carrier(change.previous_logical) ||\n            logical_uses_graphics_carrier(change.selected_logical));\n    const bool freeze_mapping_change = change.valid &&\n        (((change.mapping_changed || presentation_changed) &&\n             g_runtime.destructive_transition_failed) ||\n            frozen_carrier_change);")
research_replace(research_viewport
    "    const bool crosses_carrier_boundary = valid_native_categories &&\n        previous_category != provisional_category &&\n        (is_research_custom_category(previous_category) ||\n            is_research_custom_category(provisional_category));\n    const bool provisional_differs_from_desired =\n        g_runtime.pending.active &&\n        g_runtime.pending.top == top &&\n        g_runtime.pending.provisional_category !=\n            g_runtime.pending.desired_category;"
    "    const bool true_custom_route_involved = valid_native_categories &&\n        (is_custom_logical(change.previous_logical) ||\n            is_custom_logical(provisional_logical) ||\n            (g_runtime.pending.active &&\n                is_custom_logical(g_runtime.pending.desired_logical)));\n    const bool crosses_carrier_boundary = true_custom_route_involved &&\n        previous_category != provisional_category;\n    const bool provisional_differs_from_desired =\n        true_custom_route_involved && g_runtime.pending.active &&\n        g_runtime.pending.top == top &&\n        g_runtime.pending.provisional_category !=\n            g_runtime.pending.desired_category;")
research_replace(research_viewport
    "    if (change.valid &&\n        (change.mapping_changed || presentation_changed) &&"
    "    const bool reusable_content_changed = change.valid &&\n        change.logical_selection_changed &&\n        (logical_uses_graphics_carrier(change.previous_logical) ||\n            logical_uses_graphics_carrier(change.selected_logical));\n    if (change.valid &&\n        (change.mapping_changed || presentation_changed ||\n            reusable_content_changed) &&")

research_replace(research_viewport
    "        \"STRESS startup hooks installed: selection=eldenring.exe+0x%llX source={frame:+0x%llX pointer:+0x%llX horizontal:+0x%llX dispatch:+0x%llX cellSample:+0x%llX} nativePoll={status:%s position:+0x%llX hit:+0x%llX select:+0x%llX} compositeFrame=+0x%llX panelStateIsolation=+0x%llX->+0x%llX brightnessCarrier=selector6 textureLookup=+0x%llX imageBind=+0x%llX cross=%ls overflow=%ls alias=%ls; logical=%zu physical=%zu margin=%zu dwell={initial:%llums repeat:%llums decay:%llums minimum:%llums} imageProbe={nativeResource:dynamic-shape-rgba8 rawImageLifecycle:%s nativeResourceLifecycle:%s bitmapDataOwnership:%s pixelMutation:%s}\""
    "        \"STRESS startup hooks installed: protocol=GraphicsCarrierV2 selection=eldenring.exe+0x%llX source={frame:+0x%llX pointer:+0x%llX horizontal:+0x%llX dispatch:+0x%llX cellSample:+0x%llX} nativePoll={status:%s position:+0x%llX hit:+0x%llX select:+0x%llX} compositeFrame=+0x%llX panelStateIsolation=+0x%llX->+0x%llX rowsCarrier=category8/selector11 parking=category5 textureLookup=+0x%llX imageBind=+0x%llX cross=%ls overflow=%ls alias=%ls; logical=%zu physical=%zu margin=%zu dwell={initial:%llums repeat:%llums decay:%llums minimum:%llums} imageProbe={nativeResource:dynamic-shape-rgba8 rawImageLifecycle:%s nativeResourceLifecycle:%s bitmapDataOwnership:%s pixelMutation:%s}\"")

file(WRITE
    "${CMAKE_CURRENT_BINARY_DIR}/research-carrier/tab_viewport_probe.cpp"
    "${research_viewport}")
set(_ernativeui_tab_viewport_probe_source
    "${CMAKE_CURRENT_BINARY_DIR}/research-carrier/tab_viewport_probe.cpp")
