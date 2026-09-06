cmake_minimum_required(VERSION 3.28)

project(ERNativeUI VERSION 1.0.0 LANGUAGES C CXX) # x-release-please-version

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

set(_ernativeui_need_gfx_patch OFF)
if(ERNATIVEUI_BUILD_GFX_PATCHER OR _ernativeui_enable_tests)
    set(_ernativeui_need_gfx_patch ON)
endif()

if(_ernativeui_need_gfx_patch)
    # Portable, asset-free GFX transformation core.
    add_library(ERNativeUIGfxPatch STATIC tools/gfx_patcher/gfx_patch.cpp)
    add_library(ERNativeUI::GfxPatch ALIAS ERNativeUIGfxPatch)
    target_include_directories(ERNativeUIGfxPatch PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tools/gfx_patcher>)
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
        ER_NATIVE_UI_GFX_PATCHER_VERSION="${PROJECT_VERSION}"
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
        "mods remain supported; configure them against ERNativeUI::SDK, or set "
        "ERNATIVEUI_BUILD_HOST=OFF.")
endif()

set(_ernativeui_need_native_core OFF)
if(WIN32 AND (ERNATIVEUI_BUILD_HOST OR _ernativeui_enable_tests))
    set(_ernativeui_need_native_core ON)
endif()

if(_ernativeui_need_native_core)
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

    # Private native implementation. Client mods never link this target.
    add_library(ERNativeUICore STATIC
        src/addresses.cpp
        src/color_picker.cpp
        src/color_picker_state.cpp
        src/hooks.cpp
        src/host_storage.cpp
        src/input_binding_model.cpp
        src/menu.cpp
        src/menu_compiler.cpp
        src/module.cpp
        src/native_dialog.cpp
        src/native_input_bindings.cpp
        src/native_menu.cpp
        src/native_popup_choice.cpp
        src/native_text_input.cpp
        src/native_text_result.cpp
        src/pagination.cpp
        src/runtime.cpp
        src/runtime_log.cpp
        src/runtime_state.cpp
        src/storage_document.cpp
        src/submenu_navigation.cpp
        src/text_input_state.cpp
        src/text_registry.cpp)
    add_library(ERNativeUI::Core ALIAS ERNativeUICore)
    target_include_directories(ERNativeUICore PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(ERNativeUICore PRIVATE safetyhook::safetyhook minhook)
    target_compile_features(ERNativeUICore PUBLIC cxx_std_20)
    target_compile_definitions(ERNativeUICore PRIVATE
        ER_NATIVE_UI_BUILD_VERSION="${PROJECT_VERSION}"
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    if(_ernativeui_msvc_abi)
        target_compile_options(ERNativeUICore PRIVATE
            /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus /wd4100)
    endif()

    if(ERNATIVEUI_BUILD_HOST)
        add_library(ERNativeUIHost SHARED
            src/host_api.cpp
            src/host_dllmain.cpp
            src/host_locale.cpp
            src/host_registry.cpp
            src/logger.cpp
            src/steam_language.cpp
            src/ernativeui.rc)
        add_library(ERNativeUI::Host ALIAS ERNativeUIHost)
        target_include_directories(ERNativeUIHost PRIVATE src include)
        target_link_libraries(ERNativeUIHost PRIVATE ERNativeUI::Core)
        target_compile_features(ERNativeUIHost PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUIHost PRIVATE
            ERUI_HOST_EXPORTS
            ER_NATIVE_UI_BUILD_VERSION="${PROJECT_VERSION}"
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        set_target_properties(ERNativeUIHost PROPERTIES
            OUTPUT_NAME ERNativeUI PREFIX "")
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUIHost PRIVATE
                /W4 /EHsc /permissive- /utf-8 /Zc:__cplusplus /wd4100 /wd4324)
            target_link_options(ERNativeUIHost PRIVATE /INCREMENTAL:NO)
        endif()

    endif()

endif()

if(WIN32 AND ERNATIVEUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Keep the complete local deployment tree current even when only a document,
# locale, or optional GFX asset changed. A POST_BUILD command on the host alone
# is not rerun by an incremental build when the DLL itself is already current.
if(TARGET ERNativeUIHost)
    set(_ernativeui_stage_dependencies ERNativeUIHost)
    foreach(_ernativeui_example_target IN ITEMS
            TarnishedUIShowcase LocalizedGreeting
            ShowcaseScreenshotHelperStage MyERNativeUIMod)
        if(TARGET ${_ernativeui_example_target})
            list(APPEND _ernativeui_stage_dependencies
                ${_ernativeui_example_target})
        endif()
    endforeach()

    add_custom_target(ERNativeUIStageRuntime ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/docs"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/licenses"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/menu/win"
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
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/menu/win/02_040_optionsetting.gfx"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/menu/win/02_040_optionsetting.gfx"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/menu/win/02_042_pc_graphicsetting.gfx"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/menu/win/02_042_pc_graphicsetting.gfx"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
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
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/docs"
            "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/docs"
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

    add_executable(ERNativeUIPaginationTests tests/pagination_test.cpp src/pagination.cpp)
    target_include_directories(ERNativeUIPaginationTests PRIVATE src)
    target_compile_features(ERNativeUIPaginationTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.Pagination COMMAND ERNativeUIPaginationTests)

    add_executable(ERNativeUIModelTests
        tests/menu_model_test.cpp src/menu.cpp src/menu_compiler.cpp
        src/pagination.cpp src/text_registry.cpp)
    target_include_directories(ERNativeUIModelTests PRIVATE src include)
    target_compile_features(ERNativeUIModelTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.MenuModel COMMAND ERNativeUIModelTests)

    add_executable(ERNativeUIRuntimeOptionsTests tests/runtime_options_test.cpp)
    target_include_directories(ERNativeUIRuntimeOptionsTests PRIVATE src include)
    target_compile_features(ERNativeUIRuntimeOptionsTests PRIVATE cxx_std_20)
    add_test(NAME ERNativeUI.RuntimeOptions COMMAND ERNativeUIRuntimeOptionsTests)

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

        add_executable(ERNativeUIHostLocaleTests
            tests/host_locale_test.cpp src/host_locale.cpp)
        target_include_directories(ERNativeUIHostLocaleTests PRIVATE src include)
        target_compile_features(ERNativeUIHostLocaleTests PRIVATE cxx_std_20)
        target_compile_definitions(ERNativeUIHostLocaleTests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX
            ERNATIVEUI_TEST_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
        if(_ernativeui_msvc_abi)
            target_compile_options(ERNativeUIHostLocaleTests PRIVATE /utf-8)
        endif()
        add_test(NAME ERNativeUI.HostLocale COMMAND ERNativeUIHostLocaleTests)

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
    install(FILES assets/menu/win/02_040_optionsetting.gfx
        assets/menu/win/02_042_pc_graphicsetting.gfx
        DESTINATION ${CMAKE_INSTALL_BINDIR}/menu/win)
endif()
if(TARGET ERNativeUIGfxPatcher)
    install(TARGETS ERNativeUIGfxPatcher RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES tools/gfx_patcher/README.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI/tools/gfx_patcher)
endif()
if(TARGET TarnishedUIShowcase)
    install(TARGETS TarnishedUIShowcase LocalizedGreeting
        ShowcaseScreenshotHelper MyERNativeUIMod
        LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)
    install(FILES
        examples/showcase_screenshot_helper/ShowcaseScreenshotHelper.ini
        DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)
endif()
install(DIRECTORY examples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI/examples
    FILES_MATCHING PATTERN "*.cpp" PATTERN "*.hpp" PATTERN "*.md"
    PATTERN "*.ini" PATTERN "*.cmake" PATTERN "CMakeLists.txt")
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ERNativeUI/docs
    PATTERN "old" EXCLUDE)
install(FILES README.md CHANGELOG.md VALIDATION.md LICENSE.txt THIRD_PARTY_NOTICES.txt
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
