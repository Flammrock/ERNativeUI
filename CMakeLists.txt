cmake_minimum_required(VERSION 3.28)

project(ERNativeUI VERSION 1.3.0 LANGUAGES C CXX) # x-release-please-version

# A development-release workflow may provide a one-configure identity (for
# example, api-1.4-alpha.2+g0123456789ab) without changing the stable package
# version or teaching the public ABI about prerelease naming. Consume and clear
# the cache input so a later ordinary configure of the same build tree returns
# to PROJECT_VERSION instead of silently retaining an alpha identity.
set(_ernativeui_effective_build_identity "${PROJECT_VERSION}")
if(DEFINED ERNATIVEUI_BUILD_IDENTITY AND
        NOT ERNATIVEUI_BUILD_IDENTITY STREQUAL "")
    set(_ernativeui_effective_build_identity "${ERNATIVEUI_BUILD_IDENTITY}")
endif()
unset(ERNATIVEUI_BUILD_IDENTITY CACHE)
set(ERNATIVEUI_EFFECTIVE_BUILD_IDENTITY
    "${_ernativeui_effective_build_identity}" CACHE INTERNAL
    "Effective runtime build identity for packaging checks" FORCE)
string(LENGTH "${_ernativeui_effective_build_identity}"
    _ernativeui_build_identity_length)
if(_ernativeui_build_identity_length GREATER 96 OR
        NOT _ernativeui_effective_build_identity MATCHES
        "^[0-9A-Za-z][0-9A-Za-z._+-]*$")
    message(FATAL_ERROR
        "ERNATIVEUI_BUILD_IDENTITY must be 1-96 portable identity characters "
        "([0-9A-Za-z._+-]); got "
        "'${_ernativeui_effective_build_identity}'.")
endif()

set(_ernativeui_msvc_abi OFF)
if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(_ernativeui_msvc_abi ON)
endif()

if(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ERNativeUI must be built for x64.")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(_ernativeui_default_runtime OFF)
if(PROJECT_IS_TOP_LEVEL AND WIN32)
    set(_ernativeui_default_runtime ON)
endif()
option(ERNATIVEUI_BUILD_HOST "Build the ERNativeUI runtime host DLL"
    ${_ernativeui_default_runtime})
option(ERNATIVEUI_BUILD_EXAMPLES "Build client-mod examples"
    ${_ernativeui_default_runtime})
option(ERNATIVEUI_BUILD_GFX_PATCHER "Build the user-asset GFX patcher"
    ${PROJECT_IS_TOP_LEVEL})
option(ERNATIVEUI_BUILD_TAB_RESEARCH_PROBE
    "Build the temporary native Configuration-tab research probe" OFF)
option(ERNATIVEUI_BUILD_TAB_DUPLICATE_RESEARCH_PROBE
    "Build the temporary duplicate-category Configuration-tab probe" OFF)
option(ERNATIVEUI_BUILD_TAB_PRESENTATION_RESEARCH_PROBE
    "Build the temporary Configuration-tab presentation probe" OFF)
option(ERNATIVEUI_BUILD_TAB_VIEWPORT_RESEARCH_PROBE
    "Build the temporary Configuration-tab viewport probe" OFF)
option(ERNATIVEUI_BUILD_TAB_CONTENT_RESEARCH_PROBE
    "Build the temporary client-owned Configuration-tab content probe" OFF)
option(ERNATIVEUI_BUILD_GFX_LOAD_RESEARCH_PROBE
    "Build the observation-only Scaleform GFX-load research probe" OFF)
option(ERNATIVEUI_BUILD_TAB_CAPACITY_RESEARCH_PROBE
    "Build the observation-only live Configuration-tab capacity probe" OFF)
option(ERNATIVEUI_BUILD_TAB_SLOT_APPEND_RESEARCH_PROBE
    "Build the isolated native Configuration-tab slot-append proof" OFF)
option(ERNATIVEUI_BUILD_SCROLLBAR_STATE_RESEARCH_PROBE
    "Observe the native subpage scrollbar across an in-place row rebuild" OFF)
option(ERNATIVEUI_ENABLE_TAB_SLOT_APPEND_RESEARCH_MUTATION
    "Allow the isolated tab-slot probe to append native records up to the research target" OFF)
option(ERNATIVEUI_BUILD_GFX_TAB_GROWTH_RESEARCH_HOST
    "Build a host that requests the authored GFX tab-slot research target without changing the native tab count" OFF)
set(ERNATIVEUI_TAB_SLOT_RESEARCH_TARGET "9" CACHE STRING
    "Research-only Configuration tab-slot target (9 or 10)")
set_property(CACHE ERNATIVEUI_TAB_SLOT_RESEARCH_TARGET PROPERTY STRINGS 9 10)
if(NOT ERNATIVEUI_TAB_SLOT_RESEARCH_TARGET MATCHES "^(9|10)$")
    message(FATAL_ERROR
        "ERNATIVEUI_TAB_SLOT_RESEARCH_TARGET must be exactly 9 or 10; got "
        "'${ERNATIVEUI_TAB_SLOT_RESEARCH_TARGET}'.")
endif()
if(ERNATIVEUI_BUILD_GFX_TAB_GROWTH_RESEARCH_HOST AND
        NOT ERNATIVEUI_BUILD_HOST)
    message(FATAL_ERROR
        "ERNATIVEUI_BUILD_GFX_TAB_GROWTH_RESEARCH_HOST requires "
        "ERNATIVEUI_BUILD_HOST=ON.")
endif()
if(ERNATIVEUI_BUILD_SCROLLBAR_STATE_RESEARCH_PROBE AND
        NOT ERNATIVEUI_BUILD_HOST)
    message(FATAL_ERROR
        "ERNATIVEUI_BUILD_SCROLLBAR_STATE_RESEARCH_PROBE requires "
        "ERNATIVEUI_BUILD_HOST=ON.")
endif()
if(ERNATIVEUI_ENABLE_TAB_SLOT_APPEND_RESEARCH_MUTATION AND
        NOT ERNATIVEUI_BUILD_TAB_SLOT_APPEND_RESEARCH_PROBE)
    message(FATAL_ERROR
        "ERNATIVEUI_ENABLE_TAB_SLOT_APPEND_RESEARCH_MUTATION requires "
        "ERNATIVEUI_BUILD_TAB_SLOT_APPEND_RESEARCH_PROBE=ON.")
endif()
if(ERNATIVEUI_BUILD_TAB_CONTENT_RESEARCH_PROBE AND
        (NOT ERNATIVEUI_BUILD_HOST OR
         NOT ERNATIVEUI_BUILD_TAB_VIEWPORT_RESEARCH_PROBE))
    message(FATAL_ERROR
        "ERNATIVEUI_BUILD_TAB_CONTENT_RESEARCH_PROBE requires both "
        "ERNATIVEUI_BUILD_HOST=ON and "
        "ERNATIVEUI_BUILD_TAB_VIEWPORT_RESEARCH_PROBE=ON.")
endif()
option(ERNATIVEUI_BUILD_TESTS
    "Build ERNativeUI's tests when consumed as a subproject" OFF)
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()
set(_ernativeui_enable_tests ${ERNATIVEUI_BUILD_TESTS})
if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    set(_ernativeui_enable_tests ON)
endif()
if(_ernativeui_enable_tests)
    enable_testing()
endif()
set(ERNATIVEUI_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH
    "Directory in which runtime-ready artifacts are staged")

# Release/install trees carry a version-pinned documentation link instead of
# duplicating the repository's screenshot-heavy documentation library.
set(_ernativeui_documentation_file
    "${CMAKE_CURRENT_BINARY_DIR}/DOCUMENTATION.md")
configure_file(cmake/ERNativeUIDocumentation.md.in
    "${_ernativeui_documentation_file}" @ONLY)
set(_ernativeui_package_readme_file
    "${CMAKE_CURRENT_BINARY_DIR}/PACKAGE-README.md")
configure_file(cmake/ERNativeUIPackageReadme.md.in
    "${_ernativeui_package_readme_file}" @ONLY)

# Public headers are split into a language-neutral C ABI target and a C++17
# convenience target. Neither carries a binary link dependency.
add_library(ERNativeUICABI INTERFACE)
add_library(ERNativeUI::CABI ALIAS ERNativeUICABI)
set_target_properties(ERNativeUICABI PROPERTIES EXPORT_NAME CABI)
target_include_directories(ERNativeUICABI INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_library(ERNativeUISDK INTERFACE)
add_library(ERNativeUI::SDK ALIAS ERNativeUISDK)
set_target_properties(ERNativeUISDK PROPERTIES EXPORT_NAME SDK)
target_link_libraries(ERNativeUISDK INTERFACE ERNativeUI::CABI)
target_compile_features(ERNativeUISDK INTERFACE cxx_std_17)

# Generic, lossless GFX/SWF document model. This target intentionally has no
# dependency on the game runtime or the Elden Ring-specific settings-movie
# transformer, so its parser and edit invariants can be validated in isolation.
set(_ernativeui_need_gfx_document OFF)
if(ERNATIVEUI_BUILD_HOST OR ERNATIVEUI_BUILD_GFX_PATCHER OR
        _ernativeui_enable_tests OR
        ERNATIVEUI_BUILD_TAB_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_DUPLICATE_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_PRESENTATION_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_VIEWPORT_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_CONTENT_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_GFX_LOAD_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_CAPACITY_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_SLOT_APPEND_RESEARCH_PROBE)
    set(_ernativeui_need_gfx_document ON)
endif()

if(_ernativeui_need_gfx_document)
    add_library(ERNativeUIGfxDocument STATIC src/gfx/gfx_document.cpp)
    add_library(ERNativeUI::GfxDocument ALIAS ERNativeUIGfxDocument)
    target_include_directories(ERNativeUIGfxDocument PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
    target_compile_features(ERNativeUIGfxDocument PUBLIC cxx_std_20)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUIGfxDocument PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
    endif()

    # Elden Ring-specific semantic transforms live above the generic document
    # model. The runtime policy/cache remains platform-neutral and separately
    # testable; only the small native OpenFile adapter lives in Core.
    add_library(ERNativeUISettingsGfxTransformer STATIC
        src/gfx/settings_gfx_transformer.cpp
        src/gfx/settings_gfx_runtime.cpp)
    add_library(ERNativeUI::SettingsGfxTransformer ALIAS
        ERNativeUISettingsGfxTransformer)
    target_include_directories(ERNativeUISettingsGfxTransformer PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
    target_link_libraries(ERNativeUISettingsGfxTransformer PUBLIC
        ERNativeUI::GfxDocument)
    target_compile_features(ERNativeUISettingsGfxTransformer PUBLIC cxx_std_20)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUISettingsGfxTransformer PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
    endif()

    add_library(ERNativeUITitleGfxTransformer STATIC
        src/sha256.cpp
        src/gfx/title_gfx_transformer.cpp
        src/gfx/title_gfx_runtime.cpp)
    add_library(ERNativeUI::TitleGfxTransformer ALIAS
        ERNativeUITitleGfxTransformer)
    target_include_directories(ERNativeUITitleGfxTransformer PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
    target_link_libraries(ERNativeUITitleGfxTransformer PUBLIC
        ERNativeUI::GfxDocument)
    target_compile_features(ERNativeUITitleGfxTransformer PUBLIC cxx_std_20)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUITitleGfxTransformer PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
    endif()

endif()

set(_ernativeui_need_gfx_patch OFF)
if(ERNATIVEUI_BUILD_GFX_PATCHER OR _ernativeui_enable_tests)
    set(_ernativeui_need_gfx_patch ON)
endif()

if(_ernativeui_need_gfx_patch)
    # Backward-compatible CLI facade over the production semantic transformer.
    add_library(ERNativeUIGfxPatch STATIC
        tools/gfx_patcher/gfx_patch.cpp)
    add_library(ERNativeUI::GfxPatch ALIAS ERNativeUIGfxPatch)
    target_include_directories(ERNativeUIGfxPatch PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tools/gfx_patcher>)
    target_link_libraries(ERNativeUIGfxPatch PRIVATE
        ERNativeUI::SettingsGfxTransformer)
    target_compile_features(ERNativeUIGfxPatch PUBLIC cxx_std_20)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUIGfxPatch PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
    endif()
endif()

if(ERNATIVEUI_BUILD_GFX_PATCHER)
    add_executable(ERNativeUIGfxPatcher tools/gfx_patcher/main.cpp)
    target_link_libraries(ERNativeUIGfxPatcher PRIVATE ERNativeUI::GfxPatch)
    target_compile_features(ERNativeUIGfxPatcher PRIVATE cxx_std_20)
    target_compile_definitions(ERNativeUIGfxPatcher PRIVATE
        ER_NATIVE_UI_GFX_PATCHER_VERSION="${_ernativeui_effective_build_identity}"
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    set_target_properties(ERNativeUIGfxPatcher PROPERTIES PREFIX "")
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUIGfxPatcher PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
        target_link_options(ERNativeUIGfxPatcher PRIVATE /INCREMENTAL:NO)
    endif()
endif()

if(ERNATIVEUI_BUILD_HOST AND NOT _ernativeui_msvc_abi)
    message(FATAL_ERROR
        "ERNativeUI.dll requires an MSVC-compatible C++ ABI (MSVC or clang-cl) "
        "because Elden Ring consumes an MSVC std::function object. MinGW client "
        "source can compile against ERNativeUI::SDK; consult the selected "
        "release's tested runtime matrix, or set ERNATIVEUI_BUILD_HOST=OFF.")
endif()

set(_ernativeui_need_native_core OFF)
if(WIN32 AND (ERNATIVEUI_BUILD_HOST OR _ernativeui_enable_tests OR
        ERNATIVEUI_BUILD_TAB_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_DUPLICATE_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_PRESENTATION_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_VIEWPORT_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_CONTENT_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_GFX_LOAD_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_CAPACITY_RESEARCH_PROBE OR
        ERNATIVEUI_BUILD_TAB_SLOT_APPEND_RESEARCH_PROBE))
    set(_ernativeui_need_native_core ON)
endif()

if(_ernativeui_need_native_core)
    set(_ernativeui_hooks_source src/hooks.cpp)
    set(_ernativeui_registry_source src/host_registry.cpp)
    set(_ernativeui_tab_viewport_probe_source
        src/native_tabs.cpp)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF)
    set(SAFETYHOOK_FETCH_ZYDIS ON CACHE BOOL "" FORCE)
    set(SAFETYHOOK_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(SAFETYHOOK_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(SAFETYHOOK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(safetyhook
        GIT_REPOSITORY https://github.com/cursey/safetyhook.git
        GIT_TAG v0.7.0 GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_Declare(minhook
        GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
        GIT_TAG v1.3.4 GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(safetyhook minhook)

    if(ERNATIVEUI_BUILD_GFX_LOAD_RESEARCH_PROBE)
        if(NOT _ernativeui_msvc_abi)
            message(FATAL_ERROR
                "ERNativeUIGfxLoadProbe requires the MSVC-compatible "
                "Windows research build.")
        endif()
        add_library(ERNativeUIGfxLoadProbe SHARED
            tools/research/gfx_load_probe/gfx_load_probe.cpp)
        target_link_libraries(ERNativeUIGfxLoadProbe PRIVATE
            safetyhook::safetyhook)
        target_compile_features(ERNativeUIGfxLoadProbe PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUIGfxLoadProbe PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        set_target_properties(ERNativeUIGfxLoadProbe PROPERTIES
            PREFIX ""
            OUTPUT_NAME "ERNativeUIGfxLoadProbe")
        target_compile_options(ERNativeUIGfxLoadProbe PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
        target_link_options(ERNativeUIGfxLoadProbe PRIVATE /INCREMENTAL:NO)
    endif()

    if(ERNATIVEUI_BUILD_TAB_CAPACITY_RESEARCH_PROBE)
        if(NOT _ernativeui_msvc_abi)
            message(FATAL_ERROR
                "ERNativeUITabCapacityProbe requires the MSVC-compatible "
                "Windows research build.")
        endif()
        add_library(ERNativeUITabCapacityProbe SHARED
            tools/research/tab_capacity_probe/tab_capacity_probe.cpp
            src/module.cpp)
        target_include_directories(ERNativeUITabCapacityProbe PRIVATE src)
        target_link_libraries(ERNativeUITabCapacityProbe PRIVATE
            safetyhook::safetyhook)
        target_compile_features(ERNativeUITabCapacityProbe PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUITabCapacityProbe PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        set_target_properties(ERNativeUITabCapacityProbe PROPERTIES
            PREFIX ""
            OUTPUT_NAME "ERNativeUITabCapacityProbe")
        target_compile_options(ERNativeUITabCapacityProbe PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
        target_link_options(ERNativeUITabCapacityProbe PRIVATE /INCREMENTAL:NO)
    endif()

    if(ERNATIVEUI_BUILD_TAB_SLOT_APPEND_RESEARCH_PROBE)
        if(NOT _ernativeui_msvc_abi)
            message(FATAL_ERROR
                "ERNativeUITabSlotAppendProbe requires the MSVC-compatible "
                "Windows research build.")
        endif()
        add_library(ERNativeUITabSlotAppendProbe SHARED
            tools/research/tab_slot_append_probe/tab_slot_append_probe.cpp
            src/module.cpp)
        target_include_directories(ERNativeUITabSlotAppendProbe PRIVATE src)
        target_link_libraries(ERNativeUITabSlotAppendProbe PRIVATE
            safetyhook::safetyhook)
        target_compile_features(ERNativeUITabSlotAppendProbe PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUITabSlotAppendProbe PRIVATE
            "ER_NATIVE_UI_TAB_SLOT_RESEARCH_TARGET=${ERNATIVEUI_TAB_SLOT_RESEARCH_TARGET}"
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        if(ERNATIVEUI_ENABLE_TAB_SLOT_APPEND_RESEARCH_MUTATION)
            target_compile_definitions(ERNativeUITabSlotAppendProbe PRIVATE
                ER_NATIVE_UI_TAB_SLOT_APPEND_RESEARCH_MUTATION=1)
        endif()
        set_target_properties(ERNativeUITabSlotAppendProbe PROPERTIES
            PREFIX ""
            OUTPUT_NAME "ERNativeUITabSlotAppendProbe")
        target_compile_options(ERNativeUITabSlotAppendProbe PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
        target_link_options(ERNativeUITabSlotAppendProbe PRIVATE
            /INCREMENTAL:NO)
    endif()

    if(ERNATIVEUI_BUILD_TAB_RESEARCH_PROBE)
        add_library(ERNativeUITabProbe SHARED
            tools/research/tab_probe/tab_probe.cpp)
        target_link_libraries(ERNativeUITabProbe PRIVATE safetyhook::safetyhook)
        target_compile_features(ERNativeUITabProbe PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUITabProbe PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        set_target_properties(ERNativeUITabProbe PROPERTIES
            PREFIX ""
            OUTPUT_NAME "ERNativeUITabProbe")
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUITabProbe PRIVATE
                /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
            target_link_options(ERNativeUITabProbe PRIVATE /INCREMENTAL:NO)
        endif()
    endif()

    if(ERNATIVEUI_BUILD_TAB_DUPLICATE_RESEARCH_PROBE)
        if(NOT _ernativeui_msvc_abi)
            message(FATAL_ERROR
                "ERNativeUITabDuplicateProbe requires the MSVC-compatible "
                "Windows research build.")
        endif()
        add_library(ERNativeUITabDuplicateProbe SHARED
            tools/research/tab_probe/tab_duplicate_probe.cpp)
        target_link_libraries(ERNativeUITabDuplicateProbe PRIVATE
            safetyhook::safetyhook)
        target_compile_features(ERNativeUITabDuplicateProbe PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUITabDuplicateProbe PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        set_target_properties(ERNativeUITabDuplicateProbe PROPERTIES
            PREFIX ""
            OUTPUT_NAME "ERNativeUITabDuplicateProbe")
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUITabDuplicateProbe PRIVATE
                /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
            target_link_options(ERNativeUITabDuplicateProbe PRIVATE
                /INCREMENTAL:NO)
        endif()
    endif()

    if(ERNATIVEUI_BUILD_TAB_PRESENTATION_RESEARCH_PROBE)
        if(NOT _ernativeui_msvc_abi)
            message(FATAL_ERROR
                "ERNativeUITabPresentationProbe requires the MSVC-compatible "
                "Windows research build.")
        endif()
        add_library(ERNativeUITabPresentationProbe SHARED
            tools/research/tab_probe/tab_presentation_probe.cpp)
        target_link_libraries(ERNativeUITabPresentationProbe PRIVATE
            safetyhook::safetyhook)
        target_compile_features(ERNativeUITabPresentationProbe PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUITabPresentationProbe PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        set_target_properties(ERNativeUITabPresentationProbe PROPERTIES
            PREFIX ""
            OUTPUT_NAME "ERNativeUITabPresentationProbe")
        target_compile_options(ERNativeUITabPresentationProbe PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
        target_link_options(ERNativeUITabPresentationProbe PRIVATE
            /INCREMENTAL:NO)
    endif()

    if(ERNATIVEUI_BUILD_TAB_VIEWPORT_RESEARCH_PROBE)
        if(NOT _ernativeui_msvc_abi)
            message(FATAL_ERROR
                "ERNativeUITabViewportProbe requires the MSVC-compatible "
                "Windows research build.")
        endif()
        add_library(ERNativeUITabViewportProbe SHARED
            ${_ernativeui_tab_viewport_probe_source}
            src/native_tab_runtime_state.cpp
            src/tab_viewport.cpp)
        target_include_directories(ERNativeUITabViewportProbe PRIVATE src)
        target_link_libraries(ERNativeUITabViewportProbe PRIVATE
            safetyhook::safetyhook)
        target_compile_features(ERNativeUITabViewportProbe PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUITabViewportProbe PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        set_target_properties(ERNativeUITabViewportProbe PROPERTIES
            PREFIX ""
            OUTPUT_NAME "ERNativeUITabViewportProbe")
        target_compile_options(ERNativeUITabViewportProbe PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
        target_link_options(ERNativeUITabViewportProbe PRIVATE
            /INCREMENTAL:NO)
    endif()

    # Private native implementation. Client mods never link this target.
    add_library(ERNativeUICore STATIC
        src/addresses.cpp
        src/color_picker.cpp
        src/color_picker_state.cpp
        ${_ernativeui_hooks_source}
        src/host_storage.cpp
        src/input_binding_model.cpp
        src/menu.cpp
        src/menu_compiler.cpp
        src/module.cpp
        src/native_tab_catalogue.cpp
        src/native_tab_runtime_state.cpp
        src/native_tabs.cpp
        src/native_dialog.cpp
        src/native_input_bindings.cpp
        src/native_gfx_loader.cpp
        src/native_menu.cpp
        src/native_popup_choice.cpp
        src/native_row_presentation.cpp
        src/native_row_visibility.cpp
        src/native_slider_presentation.cpp
        src/native_text_input.cpp
        src/native_text_result.cpp
        src/pagination.cpp
        src/runtime.cpp
        src/runtime_log.cpp
        src/runtime_state.cpp
        src/row_presentation_state.cpp
        src/row_update_state.cpp
        src/row_visibility_state.cpp
        src/scaleform_html.cpp
        src/steam_dlc.cpp
        src/storage_document.cpp
        src/submenu_navigation.cpp
        src/tab_configuration.cpp
        src/tab_viewport.cpp
        src/text_input_state.cpp
        src/text_registry.cpp)
    add_library(ERNativeUI::Core ALIAS ERNativeUICore)
    target_include_directories(ERNativeUICore PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(ERNativeUICore PRIVATE
        ERNativeUI::SettingsGfxTransformer
        ERNativeUI::TitleGfxTransformer
        safetyhook::safetyhook
        minhook)
    target_compile_features(ERNativeUICore PUBLIC cxx_std_20)
    target_compile_definitions(ERNativeUICore PRIVATE
        ER_NATIVE_UI_BUILD_VERSION="${_ernativeui_effective_build_identity}"
        ER_NATIVE_UI_CUSTOM_TABS_BACKEND=1
        ER_NATIVE_UI_PRODUCTION_TABS=1
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    if(ERNATIVEUI_BUILD_TAB_CONTENT_RESEARCH_PROBE)
        target_compile_definitions(ERNativeUICore PRIVATE
            ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE=1)
    endif()
    if(ERNATIVEUI_BUILD_SCROLLBAR_STATE_RESEARCH_PROBE)
        target_compile_definitions(ERNativeUICore PRIVATE
            ER_NATIVE_UI_SCROLLBAR_STATE_RESEARCH_PROBE=1)
    endif()
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUICore PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus /wd4100)
    endif()

    if(ERNATIVEUI_BUILD_HOST)
        add_library(ERNativeUIHost SHARED
            src/host_api.cpp
            src/host_dllmain.cpp
            src/host_locale.cpp
            ${_ernativeui_registry_source}
            src/localization_config.cpp
            src/logger.cpp
            src/steam_language.cpp
            src/ernativeui.rc)
        add_library(ERNativeUI::Host ALIAS ERNativeUIHost)
        target_include_directories(ERNativeUIHost PRIVATE src include)
        target_link_libraries(ERNativeUIHost PRIVATE ERNativeUI::Core)
        target_compile_features(ERNativeUIHost PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUIHost PRIVATE
            ERUI_HOST_EXPORTS
            ER_NATIVE_UI_BUILD_VERSION="${_ernativeui_effective_build_identity}"
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        if(ERNATIVEUI_BUILD_TAB_CONTENT_RESEARCH_PROBE)
            target_compile_definitions(ERNativeUIHost PRIVATE
                ER_NATIVE_UI_TAB_CONTENT_RESEARCH_PROBE=1)
        endif()
        if(ERNATIVEUI_BUILD_GFX_TAB_GROWTH_RESEARCH_HOST)
            target_compile_definitions(ERNativeUIHost PRIVATE
                ER_NATIVE_UI_GFX_TAB_GROWTH_RESEARCH_HOST=1
                "ER_NATIVE_UI_TAB_SLOT_RESEARCH_TARGET=${ERNATIVEUI_TAB_SLOT_RESEARCH_TARGET}")
        endif()
        set_target_properties(ERNativeUIHost PROPERTIES
            OUTPUT_NAME ERNativeUI PREFIX "")
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUIHost PRIVATE
                /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus /wd4100 /wd4324)
            target_link_options(ERNativeUIHost PRIVATE /INCREMENTAL:NO)
        endif()

    endif()

    if(ERNATIVEUI_BUILD_TAB_CONTENT_RESEARCH_PROBE)
        add_library(ERNativeUITabContentClient MODULE
            tools/research/tab_probe/tab_content_client.cpp)
        target_link_libraries(ERNativeUITabContentClient PRIVATE
            ERNativeUI::SDK)
        target_compile_features(ERNativeUITabContentClient PRIVATE cxx_std_17)
        target_compile_definitions(ERNativeUITabContentClient PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        set_target_properties(ERNativeUITabContentClient PROPERTIES
            PREFIX ""
            OUTPUT_NAME "ERNativeUITabContentClient")
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUITabContentClient PRIVATE
                /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
            target_link_options(ERNativeUITabContentClient PRIVATE
                /INCREMENTAL:NO)
        endif()
    endif()

endif()

if(WIN32 AND ERNATIVEUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Keep the complete local deployment tree current even when only a document or
# locale changed. A POST_BUILD command on the host alone is not rerun by an
# incremental build when the DLL itself is already current.
if(TARGET ERNativeUIHost)
    set(_ernativeui_stage_dependencies ERNativeUIHost)
    foreach(_ernativeui_example_target IN ITEMS
            TarnishedUIShowcase LocalizedGreeting
            ShowcaseScreenshotHelperStage MyERNativeUIMod
            CustomTabsStress TitleScreenTextExample)
        if(TARGET ${_ernativeui_example_target})
            list(APPEND _ernativeui_stage_dependencies
                ${_ernativeui_example_target})
        endif()
    endforeach()

    add_custom_target(ERNativeUIStageRuntime ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/licenses"
        # Runtime transformation supersedes the two old loose-GFX overrides.
        # Remove only those legacy files: ERNATIVEUI_DEPLOY_DIR may deliberately
        # target a live mod tree containing unrelated menu replacements.
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/menu/win/02_040_optionsetting.gfx"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/menu/win/02_042_pc_graphicsetting.gfx"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ERNativeUIHost>"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/ERNativeUI.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/ERNativeUI.ini"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/ERNativeUI.ini"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/locales"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_ernativeui_package_readme_file}"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/README.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/CHANGELOG.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/VALIDATION.md"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/VALIDATION.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/THIRD_PARTY_NOTICES.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_ernativeui_documentation_file}"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/DOCUMENTATION.md"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/examples"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${safetyhook_SOURCE_DIR}/LICENSE"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/licenses/SafetyHook-LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${minhook_SOURCE_DIR}/LICENSE.txt"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/licenses/MinHook-LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${Zydis_SOURCE_DIR}/LICENSE"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/licenses/Zydis-LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${Zycore_SOURCE_DIR}/LICENSE"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/licenses/Zycore-LICENSE.txt"
        DEPENDS ${_ernativeui_stage_dependencies}
        VERBATIM)
    unset(_ernativeui_stage_dependencies)
    unset(_ernativeui_example_target)
endif()

if(NOT WIN32)
    message(STATUS "Non-Windows build: public headers and selected portable tools only.")
endif()

if(_ernativeui_enable_tests)
    if(WIN32)
        # Game-independent DLL built from the real production negotiation and
        # registry sources. Frozen clients load this target in isolated test
        # processes; no Elden Ring executable or native hook installation is
        # involved.
        add_library(ERNativeUIAbiHostCurrent SHARED
            tests/abi/support/current_host_dllmain.cpp
            src/host_api.cpp
            src/host_registry.cpp
            src/steam_language.cpp)
        target_include_directories(ERNativeUIAbiHostCurrent PRIVATE
            src include)
        target_link_libraries(ERNativeUIAbiHostCurrent PRIVATE
            ERNativeUI::Core)
        target_compile_features(ERNativeUIAbiHostCurrent PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUIAbiHostCurrent PRIVATE
            ERUI_HOST_EXPORTS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        set_target_properties(ERNativeUIAbiHostCurrent PROPERTIES
            OUTPUT_NAME ERNativeUI
            PREFIX ""
            RUNTIME_OUTPUT_DIRECTORY
                "${CMAKE_BINARY_DIR}/abi/runtime/current"
            ARCHIVE_OUTPUT_DIRECTORY
                "${CMAKE_BINARY_DIR}/abi/runtime/current"
            LIBRARY_OUTPUT_DIRECTORY
                "${CMAKE_BINARY_DIR}/abi/runtime/current")
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUIAbiHostCurrent PRIVATE
                /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus /wd4100 /wd4324)
            target_link_options(ERNativeUIAbiHostCurrent PRIVATE
                /INCREMENTAL:NO)
        endif()

        # Lifecycle fixture built from the same production API, registry, and
        # Steam-language sources. Its tiny DllMain deliberately holds the host
        # in the real "registration open, language pending" startup interval.
        add_library(ERNativeUIAbiHostV1_0Pending SHARED
            tests/abi/support/pending_v1_0_host_dllmain.cpp
            src/host_api.cpp
            src/host_registry.cpp
            src/steam_language.cpp)
        target_include_directories(ERNativeUIAbiHostV1_0Pending PRIVATE
            src include tests/abi/support)
        target_link_libraries(ERNativeUIAbiHostV1_0Pending PRIVATE
            ERNativeUI::Core)
        target_compile_features(ERNativeUIAbiHostV1_0Pending PRIVATE
            cxx_std_20)
        target_compile_definitions(ERNativeUIAbiHostV1_0Pending PRIVATE
            ERUI_HOST_EXPORTS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        set_target_properties(ERNativeUIAbiHostV1_0Pending PROPERTIES
            OUTPUT_NAME ERNativeUI
            PREFIX ""
            RUNTIME_OUTPUT_DIRECTORY
                "${CMAKE_BINARY_DIR}/abi/runtime/v1_0_pending"
            ARCHIVE_OUTPUT_DIRECTORY
                "${CMAKE_BINARY_DIR}/abi/runtime/v1_0_pending"
            LIBRARY_OUTPUT_DIRECTORY
                "${CMAKE_BINARY_DIR}/abi/runtime/v1_0_pending")
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUIAbiHostV1_0Pending PRIVATE
                /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus /wd4100 /wd4324)
            target_link_options(ERNativeUIAbiHostV1_0Pending PRIVATE
                /INCREMENTAL:NO)
        endif()
    endif()

    add_subdirectory(tests/abi)

    add_executable(ERNativeUIGfxPatcherTests tests/gfx_patcher_test.cpp)
    target_link_libraries(ERNativeUIGfxPatcherTests PRIVATE ERNativeUI::GfxPatch)
    target_compile_definitions(ERNativeUIGfxPatcherTests PRIVATE
        ERNATIVEUI_TEST_GAME_OPTIONS_GFX="${CMAKE_CURRENT_SOURCE_DIR}/assets/menu/win/02_040_optionsetting.gfx"
        ERNATIVEUI_TEST_ADVANCED_GFX="${CMAKE_CURRENT_SOURCE_DIR}/assets/menu/win/02_042_pc_graphicsetting.gfx")
    target_compile_features(ERNativeUIGfxPatcherTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.GfxPatcher COMMAND ERNativeUIGfxPatcherTests)

    add_executable(ERNativeUIGfxDocumentTests tests/gfx_document_test.cpp)
    target_link_libraries(ERNativeUIGfxDocumentTests PRIVATE
        ERNativeUI::GfxDocument)
    target_include_directories(ERNativeUIGfxDocumentTests PRIVATE tests)
    target_compile_definitions(ERNativeUIGfxDocumentTests PRIVATE
        ERNATIVEUI_TEST_GAME_OPTIONS_GFX="${CMAKE_CURRENT_SOURCE_DIR}/assets/menu/win/02_040_optionsetting.gfx"
        ERNATIVEUI_TEST_ADVANCED_GFX="${CMAKE_CURRENT_SOURCE_DIR}/assets/menu/win/02_042_pc_graphicsetting.gfx")
    target_compile_features(ERNativeUIGfxDocumentTests PRIVATE cxx_std_20)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUIGfxDocumentTests PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME ERNativeUI.GfxDocument COMMAND ERNativeUIGfxDocumentTests)

    add_executable(ERNativeUISettingsGfxTransformerTests
        tests/settings_gfx_transformer_test.cpp)
    target_link_libraries(ERNativeUISettingsGfxTransformerTests PRIVATE
        ERNativeUI::SettingsGfxTransformer)
    target_include_directories(ERNativeUISettingsGfxTransformerTests PRIVATE
        tests)
    target_compile_definitions(ERNativeUISettingsGfxTransformerTests PRIVATE
        ERNATIVEUI_TEST_GAME_OPTIONS_GFX="${CMAKE_CURRENT_SOURCE_DIR}/assets/menu/win/02_040_optionsetting.gfx"
        ERNATIVEUI_TEST_ADVANCED_GFX="${CMAKE_CURRENT_SOURCE_DIR}/assets/menu/win/02_042_pc_graphicsetting.gfx")
    target_compile_features(ERNativeUISettingsGfxTransformerTests PRIVATE
        cxx_std_20)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUISettingsGfxTransformerTests PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME ERNativeUI.SettingsGfxTransformer
        COMMAND ERNativeUISettingsGfxTransformerTests)

    add_executable(ERNativeUISettingsGfxRuntimeTests
        tests/settings_gfx_runtime_test.cpp)
    target_link_libraries(ERNativeUISettingsGfxRuntimeTests PRIVATE
        ERNativeUI::SettingsGfxTransformer)
    target_include_directories(ERNativeUISettingsGfxRuntimeTests PRIVATE
        tests)
    target_compile_definitions(ERNativeUISettingsGfxRuntimeTests PRIVATE
        ERNATIVEUI_TEST_GAME_OPTIONS_GFX="${CMAKE_CURRENT_SOURCE_DIR}/assets/menu/win/02_040_optionsetting.gfx"
        ERNATIVEUI_TEST_ADVANCED_GFX="${CMAKE_CURRENT_SOURCE_DIR}/assets/menu/win/02_042_pc_graphicsetting.gfx")
    target_compile_features(ERNativeUISettingsGfxRuntimeTests PRIVATE
        cxx_std_20)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUISettingsGfxRuntimeTests PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME ERNativeUI.SettingsGfxRuntime
        COMMAND ERNativeUISettingsGfxRuntimeTests)

    add_executable(ERNativeUITitleGfxTransformerTests
        tests/title_gfx_transformer_test.cpp)
    target_link_libraries(ERNativeUITitleGfxTransformerTests PRIVATE
        ERNativeUI::TitleGfxTransformer)
    target_include_directories(ERNativeUITitleGfxTransformerTests PRIVATE
        tests)
    target_compile_features(ERNativeUITitleGfxTransformerTests PRIVATE
        cxx_std_20)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUITitleGfxTransformerTests PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME ERNativeUI.TitleGfxTransformer
        COMMAND ERNativeUITitleGfxTransformerTests)

    add_executable(ERNativeUITitleGfxRuntimeTests
        tests/title_gfx_runtime_test.cpp)
    target_link_libraries(ERNativeUITitleGfxRuntimeTests PRIVATE
        ERNativeUI::TitleGfxTransformer)
    target_include_directories(ERNativeUITitleGfxRuntimeTests PRIVATE tests)
    target_compile_features(ERNativeUITitleGfxRuntimeTests PRIVATE cxx_std_20)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUITitleGfxRuntimeTests PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME ERNativeUI.TitleGfxRuntime
        COMMAND ERNativeUITitleGfxRuntimeTests)

    add_executable(ERNativeUISha256Tests tests/sha256_test.cpp)
    target_link_libraries(ERNativeUISha256Tests PRIVATE
        ERNativeUI::TitleGfxTransformer)
    target_include_directories(ERNativeUISha256Tests PRIVATE tests)
    target_compile_features(ERNativeUISha256Tests PRIVATE cxx_std_20)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUISha256Tests PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME ERNativeUI.Sha256 COMMAND ERNativeUISha256Tests)

    add_executable(ERNativeUIPaginationTests tests/pagination_test.cpp src/pagination.cpp)
    target_include_directories(ERNativeUIPaginationTests PRIVATE src)
    target_compile_features(ERNativeUIPaginationTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.Pagination COMMAND ERNativeUIPaginationTests)

    add_executable(ERNativeUITabViewportTests
        tests/tab_viewport_test.cpp src/tab_viewport.cpp)
    target_include_directories(ERNativeUITabViewportTests PRIVATE
        src include tests)
    target_compile_features(ERNativeUITabViewportTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.TabViewport COMMAND ERNativeUITabViewportTests)

    add_executable(ERNativeUITabPointerDwellTests
        tests/tab_pointer_dwell_test.cpp src/tab_viewport.cpp)
    target_include_directories(ERNativeUITabPointerDwellTests PRIVATE src tests)
    target_compile_features(ERNativeUITabPointerDwellTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.TabPointerDwell
        COMMAND ERNativeUITabPointerDwellTests)

    add_executable(ERNativeUITabRecordEpochTests
        tests/tab_record_epoch_test.cpp)
    target_include_directories(ERNativeUITabRecordEpochTests PRIVATE src tests)
    target_compile_features(ERNativeUITabRecordEpochTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.TabRecordEpoch COMMAND ERNativeUITabRecordEpochTests)

    add_executable(ERNativeUITabTransitionInputGateTests
        tests/tab_transition_input_gate_test.cpp)
    target_include_directories(ERNativeUITabTransitionInputGateTests PRIVATE src tests)
    target_compile_features(ERNativeUITabTransitionInputGateTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.TabTransitionInputGate
        COMMAND ERNativeUITabTransitionInputGateTests)

    add_executable(ERNativeUITabIconRetirementPolicyTests
        tests/tab_icon_retirement_policy_test.cpp)
    target_include_directories(ERNativeUITabIconRetirementPolicyTests PRIVATE
        src tests)
    target_compile_features(ERNativeUITabIconRetirementPolicyTests PRIVATE
        cxx_std_20)
    add_test(NAME ERNativeUI.TabIconRetirementPolicy
        COMMAND ERNativeUITabIconRetirementPolicyTests)

    add_executable(ERNativeUITabConfigurationTests
        tests/tab_configuration_test.cpp src/tab_configuration.cpp)
    target_include_directories(ERNativeUITabConfigurationTests PRIVATE
        src tests)
    target_compile_features(ERNativeUITabConfigurationTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.TabConfiguration
        COMMAND ERNativeUITabConfigurationTests)

    add_executable(ERNativeUINativeTabCatalogueTests
        tests/native_tab_catalogue_test.cpp
        src/menu.cpp
        src/menu_compiler.cpp
        src/native_tab_catalogue.cpp
        src/pagination.cpp
        src/tab_configuration.cpp
        src/text_registry.cpp)
    target_include_directories(ERNativeUINativeTabCatalogueTests PRIVATE
        src include tests)
    target_compile_features(ERNativeUINativeTabCatalogueTests PRIVATE
        cxx_std_20)
    add_test(NAME ERNativeUI.NativeTabCatalogue
        COMMAND ERNativeUINativeTabCatalogueTests)

    add_executable(ERNativeUINativeTabRuntimeStateTests
        tests/native_tab_runtime_state_test.cpp
        src/native_tab_runtime_state.cpp)
    target_include_directories(ERNativeUINativeTabRuntimeStateTests PRIVATE
        src tests)
    target_compile_features(ERNativeUINativeTabRuntimeStateTests PRIVATE
        cxx_std_20)
    add_test(NAME ERNativeUI.NativeTabRuntimeState
        COMMAND ERNativeUINativeTabRuntimeStateTests)

    add_executable(ERNativeUIModelTests
        tests/menu_model_test.cpp src/menu.cpp src/menu_compiler.cpp
        src/pagination.cpp src/text_registry.cpp)
    target_include_directories(ERNativeUIModelTests PRIVATE src include)
    target_compile_features(ERNativeUIModelTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.MenuModel COMMAND ERNativeUIModelTests)

    add_executable(ERNativeUIRowPresentationStateTests
        tests/row_presentation_state_test.cpp
        src/menu.cpp
        src/menu_compiler.cpp
        src/pagination.cpp
        src/row_presentation_state.cpp
        src/text_registry.cpp)
    target_include_directories(ERNativeUIRowPresentationStateTests PRIVATE
        src include)
    target_compile_features(ERNativeUIRowPresentationStateTests PRIVATE
        cxx_std_20)
    add_test(NAME ERNativeUI.RowPresentationState
        COMMAND ERNativeUIRowPresentationStateTests)

    add_executable(ERNativeUISelectedHelpRefreshStateTests
        tests/selected_help_refresh_state_test.cpp)
    target_include_directories(ERNativeUISelectedHelpRefreshStateTests PRIVATE
        src include tests)
    target_compile_features(ERNativeUISelectedHelpRefreshStateTests PRIVATE
        cxx_std_20)
    add_test(NAME ERNativeUI.SelectedHelpRefreshState
        COMMAND ERNativeUISelectedHelpRefreshStateTests)

    add_executable(ERNativeUIRowVisibilityStateTests
        tests/row_visibility_state_test.cpp
        src/menu.cpp
        src/menu_compiler.cpp
        src/pagination.cpp
        src/row_visibility_state.cpp
        src/text_registry.cpp)
    target_include_directories(ERNativeUIRowVisibilityStateTests PRIVATE
        src include)
    target_compile_features(ERNativeUIRowVisibilityStateTests PRIVATE
        cxx_std_20)
    add_test(NAME ERNativeUI.RowVisibilityState
        COMMAND ERNativeUIRowVisibilityStateTests)

    add_executable(ERNativeUIScaleformHtmlTests
        tests/scaleform_html_test.cpp
        src/scaleform_html.cpp)
    target_include_directories(ERNativeUIScaleformHtmlTests PRIVATE
        src include)
    target_compile_features(ERNativeUIScaleformHtmlTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.ScaleformHtml
        COMMAND ERNativeUIScaleformHtmlTests)

    add_executable(ERNativeUIRuntimeOptionsTests tests/runtime_options_test.cpp)
    target_include_directories(ERNativeUIRuntimeOptionsTests PRIVATE src include)
    target_compile_features(ERNativeUIRuntimeOptionsTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.RuntimeOptions COMMAND ERNativeUIRuntimeOptionsTests)

    add_executable(ERNativeUILoggingPolicyTests
        tests/logging_policy_test.cpp
        src/runtime_log.cpp)
    target_include_directories(ERNativeUILoggingPolicyTests PRIVATE
        src include tests)
    target_compile_features(ERNativeUILoggingPolicyTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.LoggingPolicy COMMAND ERNativeUILoggingPolicyTests)

    if(WIN32)
        add_executable(ERNativeUISharedMenuAddressTests
            tests/shared_menu_address_test.cpp
            src/addresses.cpp
            src/module.cpp
            src/runtime_log.cpp)
        target_include_directories(ERNativeUISharedMenuAddressTests PRIVATE
            src include tests)
        target_compile_features(ERNativeUISharedMenuAddressTests PRIVATE
            cxx_std_20)
        target_compile_definitions(ERNativeUISharedMenuAddressTests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUISharedMenuAddressTests PRIVATE
                /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus)
        endif()
        add_test(NAME ERNativeUI.SharedMenuAddresses
            COMMAND ERNativeUISharedMenuAddressTests)

        add_executable(ERNativeUILoggerIntegrationTests
            tests/logger_integration_test.cpp
            src/logger.cpp)
        target_include_directories(ERNativeUILoggerIntegrationTests PRIVATE
            src include tests)
        target_compile_features(ERNativeUILoggerIntegrationTests PRIVATE
            cxx_std_20)
        target_compile_definitions(ERNativeUILoggerIntegrationTests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        add_test(NAME ERNativeUI.LoggerIntegration
            COMMAND ERNativeUILoggerIntegrationTests)
    endif()

    add_executable(ERNativeUIGameBuildTests tests/game_build_test.cpp)
    target_include_directories(ERNativeUIGameBuildTests PRIVATE src tests)
    target_compile_features(ERNativeUIGameBuildTests PRIVATE cxx_std_17)
    add_test(NAME ERNativeUI.GameBuild COMMAND ERNativeUIGameBuildTests)

    add_executable(ERNativeUIDialogModalStateTests
        tests/dialog_modal_state_test.cpp)
    target_include_directories(ERNativeUIDialogModalStateTests PRIVATE src)
    target_compile_features(ERNativeUIDialogModalStateTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.DialogModalState
        COMMAND ERNativeUIDialogModalStateTests)

    add_executable(ERNativeUIBindingRuntimeStateTests
        tests/binding_runtime_state_test.cpp)
    target_include_directories(ERNativeUIBindingRuntimeStateTests PRIVATE src)
    target_compile_features(ERNativeUIBindingRuntimeStateTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.BindingRuntimeState
        COMMAND ERNativeUIBindingRuntimeStateTests)

    add_executable(ERNativeUIInputBindingModelTests
        tests/input_binding_model_test.cpp
        src/input_binding_model.cpp)
    target_include_directories(ERNativeUIInputBindingModelTests PRIVATE
        src include)
    target_compile_features(ERNativeUIInputBindingModelTests PRIVATE
        cxx_std_17)
    add_test(NAME ERNativeUI.InputBindingModel
        COMMAND ERNativeUIInputBindingModelTests)

    add_executable(ERNativeUIDialogPresentationTests
        tests/native_dialog_presentation_test.cpp)
    target_include_directories(ERNativeUIDialogPresentationTests PRIVATE
        src include)
    target_compile_features(ERNativeUIDialogPresentationTests PRIVATE
        cxx_std_17)
    add_test(NAME ERNativeUI.DialogPresentation
        COMMAND ERNativeUIDialogPresentationTests)

    add_executable(ERNativeUIPopupChoiceStateTests
        tests/popup_choice_state_test.cpp)
    target_include_directories(ERNativeUIPopupChoiceStateTests PRIVATE src)
    target_compile_features(ERNativeUIPopupChoiceStateTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.PopupChoiceState
        COMMAND ERNativeUIPopupChoiceStateTests)

    add_executable(ERNativeUIColorPickerStateTests
        tests/color_picker_state_test.cpp
        src/color_picker_state.cpp)
    target_include_directories(ERNativeUIColorPickerStateTests PRIVATE src)
    target_compile_features(ERNativeUIColorPickerStateTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.ColorPickerState
        COMMAND ERNativeUIColorPickerStateTests)

    add_executable(ERNativeUIAbiCCompileTests tests/abi_c_compile_test.c)
    target_link_libraries(ERNativeUIAbiCCompileTests PRIVATE ERNativeUI::CABI)
    set_target_properties(ERNativeUIAbiCCompileTests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
    add_test(NAME ERNativeUI.AbiCCompile COMMAND ERNativeUIAbiCCompileTests)

    add_executable(ERNativeUIPagePresentationCTests
        tests/page_presentation_c_test.c)
    target_link_libraries(ERNativeUIPagePresentationCTests PRIVATE
        ERNativeUI::CABI)
    set_target_properties(ERNativeUIPagePresentationCTests PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
    add_test(NAME ERNativeUI.PagePresentationC
        COMMAND ERNativeUIPagePresentationCTests)

    if(WIN32)
        add_executable(ERNativeUITextResultTests
            tests/native_text_result_test.cpp src/native_text_result.cpp)
        target_include_directories(ERNativeUITextResultTests PRIVATE src)
        target_compile_features(ERNativeUITextResultTests PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUITextResultTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        add_test(NAME ERNativeUI.NativeTextResult COMMAND ERNativeUITextResultTests)

        add_executable(ERNativeUIButtonBridgeTests tests/native_button_bridge_test.cpp)
        target_include_directories(ERNativeUIButtonBridgeTests PRIVATE src include)
        target_compile_features(ERNativeUIButtonBridgeTests PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUIButtonBridgeTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(ERNativeUIButtonBridgeTests PRIVATE ERNativeUI::Core)
        add_test(NAME ERNativeUI.NativeButtonBridge COMMAND ERNativeUIButtonBridgeTests)

        add_executable(ERNativeUISubmenuBridgeTests tests/native_submenu_bridge_test.cpp)
        target_include_directories(ERNativeUISubmenuBridgeTests PRIVATE src include)
        target_compile_features(ERNativeUISubmenuBridgeTests PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUISubmenuBridgeTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(ERNativeUISubmenuBridgeTests PRIVATE ERNativeUI::Core)
        add_test(NAME ERNativeUI.NativeSubmenuBridge COMMAND ERNativeUISubmenuBridgeTests)

        add_executable(ERNativeUISubmenuNavigationTests
            tests/submenu_navigation_test.cpp src/submenu_navigation.cpp)
        target_include_directories(ERNativeUISubmenuNavigationTests PRIVATE src)
        target_compile_features(ERNativeUISubmenuNavigationTests PRIVATE cxx_std_20)
        add_test(NAME ERNativeUI.SubmenuNavigation COMMAND ERNativeUISubmenuNavigationTests)

        add_executable(ERNativeUITitleBridgeTests
            tests/native_title_bridge_test.cpp)
        target_include_directories(ERNativeUITitleBridgeTests PRIVATE src)
        target_compile_features(ERNativeUITitleBridgeTests PRIVATE cxx_std_20)
        add_test(NAME ERNativeUI.TitleBridge COMMAND ERNativeUITitleBridgeTests)

        add_executable(ERNativeUIClientHeaderTests tests/client_header_test.cpp)
        target_link_libraries(ERNativeUIClientHeaderTests PRIVATE ERNativeUI::SDK)
        target_compile_features(ERNativeUIClientHeaderTests PRIVATE cxx_std_17)
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUIClientHeaderTests PRIVATE /Zc:__cplusplus)
        endif()
        add_test(NAME ERNativeUI.ClientHeader COMMAND ERNativeUIClientHeaderTests)

        add_executable(ERNativeUISteamLanguageTests
            tests/steam_language_test.cpp src/steam_language.cpp)
        target_include_directories(ERNativeUISteamLanguageTests PRIVATE
            src include)
        target_compile_features(ERNativeUISteamLanguageTests PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUISteamLanguageTests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX)
        add_test(NAME ERNativeUI.SteamLanguage
            COMMAND ERNativeUISteamLanguageTests)

        add_executable(ERNativeUISteamLanguageUnavailableTests
            tests/steam_language_unavailable_test.cpp src/steam_language.cpp)
        target_include_directories(
            ERNativeUISteamLanguageUnavailableTests PRIVATE src include)
        target_compile_features(
            ERNativeUISteamLanguageUnavailableTests PRIVATE cxx_std_20)
        target_compile_definitions(
            ERNativeUISteamLanguageUnavailableTests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX)
        add_test(NAME ERNativeUI.SteamLanguageUnavailable
            COMMAND ERNativeUISteamLanguageUnavailableTests)

        add_executable(ERNativeUISteamDlcTests
            tests/steam_dlc_test.cpp src/steam_dlc.cpp)
        target_include_directories(ERNativeUISteamDlcTests PRIVATE src tests)
        target_compile_features(ERNativeUISteamDlcTests PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUISteamDlcTests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX)
        add_test(NAME ERNativeUI.SteamDlc COMMAND ERNativeUISteamDlcTests)

        add_executable(ERNativeUIHostLocaleTests
            tests/host_locale_test.cpp
            src/host_locale.cpp
            src/localization_config.cpp)
        target_include_directories(ERNativeUIHostLocaleTests PRIVATE src include)
        target_compile_features(ERNativeUIHostLocaleTests PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUIHostLocaleTests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX
            ERNATIVEUI_TEST_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUIHostLocaleTests PRIVATE /utf-8)
        endif()
        add_test(NAME ERNativeUI.HostLocale COMMAND ERNativeUIHostLocaleTests)

        add_executable(ERNativeUILocalizationConfigTests
            tests/localization_config_test.cpp src/localization_config.cpp)
        target_include_directories(
            ERNativeUILocalizationConfigTests PRIVATE src include)
        target_compile_features(
            ERNativeUILocalizationConfigTests PRIVATE cxx_std_20)
        add_test(NAME ERNativeUI.LocalizationConfig
            COMMAND ERNativeUILocalizationConfigTests)

        add_executable(ERNativeUIStorageDocumentTests
            tests/storage_document_test.cpp src/storage_document.cpp)
        target_include_directories(ERNativeUIStorageDocumentTests PRIVATE
            src include)
        target_compile_features(ERNativeUIStorageDocumentTests PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUIStorageDocumentTests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        add_test(NAME ERNativeUI.StorageDocument
            COMMAND ERNativeUIStorageDocumentTests)

        add_executable(ERNativeUIHostStorageTests
            tests/host_storage_test.cpp
            src/host_storage.cpp
            src/input_binding_model.cpp
            src/storage_document.cpp)
        target_include_directories(ERNativeUIHostStorageTests PRIVATE
            src include)
        target_compile_features(ERNativeUIHostStorageTests PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUIHostStorageTests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        add_test(NAME ERNativeUI.HostStorage
            COMMAND ERNativeUIHostStorageTests)

        add_executable(ERNativeUIPagePresentationCppTests
            tests/page_presentation_cpp_test.cpp)
        target_link_libraries(ERNativeUIPagePresentationCppTests PRIVATE
            ERNativeUI::SDK)
        target_compile_features(ERNativeUIPagePresentationCppTests PRIVATE
            cxx_std_17)
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUIPagePresentationCppTests PRIVATE
                /Zc:__cplusplus)
        endif()
        add_test(NAME ERNativeUI.PagePresentationCpp
            COMMAND ERNativeUIPagePresentationCppTests)

        add_executable(ERNativeUIRegistryTests
            tests/host_registry_test.cpp src/host_api.cpp
            src/host_registry.cpp src/menu.cpp src/steam_language.cpp)
        target_include_directories(ERNativeUIRegistryTests PRIVATE src include)
        target_compile_features(ERNativeUIRegistryTests PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUIRegistryTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(ERNativeUIRegistryTests PRIVATE ERNativeUI::Core)
        add_test(NAME ERNativeUI.Registry COMMAND ERNativeUIRegistryTests)

        add_executable(ERNativeUISliderRegistryTests
            tests/slider_registry_test.cpp
            src/host_registry.cpp src/menu.cpp src/steam_language.cpp)
        target_include_directories(ERNativeUISliderRegistryTests PRIVATE
            src include tests)
        target_compile_features(ERNativeUISliderRegistryTests PRIVATE
            cxx_std_20)
        target_compile_definitions(ERNativeUISliderRegistryTests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(ERNativeUISliderRegistryTests PRIVATE
            ERNativeUI::Core)
        add_test(NAME ERNativeUI.SliderRegistry
            COMMAND ERNativeUISliderRegistryTests)
    endif()
endif()

install(DIRECTORY include/ernativeui/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ernativeui)
install(TARGETS ERNativeUICABI ERNativeUISDK EXPORT ERNativeUITargets)
install(EXPORT ERNativeUITargets
    FILE ERNativeUITargets.cmake NAMESPACE ERNativeUI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ERNativeUI)
if(TARGET ERNativeUIHost)
    install(TARGETS ERNativeUIHost RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES assets/ERNativeUI.ini DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY assets/locales/
        DESTINATION ${CMAKE_INSTALL_BINDIR}/locales)
endif()
if(TARGET TarnishedUIShowcase)
    install(TARGETS TarnishedUIShowcase LocalizedGreeting
        ShowcaseScreenshotHelper MyERNativeUIMod CustomTabsStress
        TitleScreenTextExample
        LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)
    install(FILES
        examples/showcase_screenshot_helper/ShowcaseScreenshotHelper.ini
        DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)
endif()
install(DIRECTORY examples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI/examples
    FILES_MATCHING PATTERN "*.cpp" PATTERN "*.hpp" PATTERN "*.md"
    PATTERN "*.ini" PATTERN "*.cmake" PATTERN "CMakeLists.txt")
install(FILES "${_ernativeui_documentation_file}"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI)
install(FILES "${_ernativeui_package_readme_file}"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI
    RENAME README.md)
install(FILES CHANGELOG.md VALIDATION.md LICENSE.txt THIRD_PARTY_NOTICES.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI)
if(TARGET ERNativeUIHost)
    install(FILES "${safetyhook_SOURCE_DIR}/LICENSE"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI/licenses
        RENAME SafetyHook-LICENSE.txt)
    install(FILES "${minhook_SOURCE_DIR}/LICENSE.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI/licenses
        RENAME MinHook-LICENSE.txt)
    install(FILES "${Zydis_SOURCE_DIR}/LICENSE"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI/licenses
        RENAME Zydis-LICENSE.txt)
    install(FILES "${Zycore_SOURCE_DIR}/LICENSE"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI/licenses
        RENAME Zycore-LICENSE.txt)
endif()

configure_package_config_file(cmake/ERNativeUIConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/ERNativeUIConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ERNativeUI)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ERNativeUIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ERNativeUIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ERNativeUIConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ERNativeUI)

if(PROJECT_IS_TOP_LEVEL)
    set(CPACK_PACKAGE_NAME "ERNativeUI")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    if(WIN32)
        set(CPACK_PACKAGE_FILE_NAME "ERNativeUI-${PROJECT_VERSION}-windows-x64")
    else()
        set(CPACK_PACKAGE_FILE_NAME "ERNativeUI-${PROJECT_VERSION}-source-sdk")
    endif()
    set(CPACK_PACKAGE_VENDOR "ERNativeUI contributors")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_SOURCE_GENERATOR "ZIP")
    set(CPACK_SOURCE_IGNORE_FILES
        "/build[^/]*/"
        "/dist/"
        "/_CPack_Packages/"
        "/gfx_fingerprint_output/"
        "/gfx_patch_output/"
        "/\\.git/"
        "\\.swf$"
        "\\.zip$"
        "\\.log$")
    include(CPack)
endif()
