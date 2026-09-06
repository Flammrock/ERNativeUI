cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ERNativeUIAbiTests LANGUAGES C CXX)
    include(CTest)
    enable_testing()
endif()

get_filename_component(ERNATIVEUI_ABI_REPOSITORY_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(ERNATIVEUI_ABI_SUPPORT_DIR "${CMAKE_CURRENT_LIST_DIR}/support")

add_executable(ERNativeUIAbiCurrentCLayout current/c_layout_test.c)
target_include_directories(ERNativeUIAbiCurrentCLayout PRIVATE
    "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include")
set_target_properties(ERNativeUIAbiCurrentCLayout PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO)
add_test(NAME ERNativeUI.ABI.Current.CLayout
    COMMAND ERNativeUIAbiCurrentCLayout)

add_executable(ERNativeUIAbiCurrentActionInputsCodecC
    current/action_inputs_codec_c_test.c
    current/action_inputs_codec_c_other.c)
target_include_directories(ERNativeUIAbiCurrentActionInputsCodecC PRIVATE
    "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include")
set_target_properties(ERNativeUIAbiCurrentActionInputsCodecC PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO)
add_test(NAME ERNativeUI.ABI.Current.ActionInputsCodecC
    COMMAND ERNativeUIAbiCurrentActionInputsCodecC)

if(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "ERNativeUI ABI runtime tests require Windows x64.")
    endif()

    add_executable(ERNativeUIAbiCurrentCppHeader
        current/cpp_header_test.cpp)
    target_include_directories(ERNativeUIAbiCurrentCppHeader PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include")
    target_compile_features(ERNativeUIAbiCurrentCppHeader PRIVATE cxx_std_17)
    target_compile_definitions(ERNativeUIAbiCurrentCppHeader PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(ERNativeUIAbiCurrentCppHeader PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()
    add_test(NAME ERNativeUI.ABI.Current.CppHeader
        COMMAND ERNativeUIAbiCurrentCppHeader)

    add_executable(ERNativeUIAbiCurrentActionInputsCodecCpp
        current/action_inputs_codec_cpp_test.cpp)
    target_include_directories(ERNativeUIAbiCurrentActionInputsCodecCpp PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include")
    target_compile_features(ERNativeUIAbiCurrentActionInputsCodecCpp PRIVATE
        cxx_std_17)
    target_compile_definitions(ERNativeUIAbiCurrentActionInputsCodecCpp PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(ERNativeUIAbiCurrentActionInputsCodecCpp PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()
    add_test(NAME ERNativeUI.ABI.Current.ActionInputsCodecCpp
        COMMAND ERNativeUIAbiCurrentActionInputsCodecCpp)

    add_executable(ERNativeUIAbiNegotiationCurrent
        current/api_negotiation_test.cpp
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiNegotiationCurrent PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(ERNativeUIAbiNegotiationCurrent PRIVATE cxx_std_17)
    set_target_properties(ERNativeUIAbiNegotiationCurrent PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES)
    target_compile_definitions(ERNativeUIAbiNegotiationCurrent PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(ERNativeUIAbiNegotiationCurrent PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()

    add_executable(ERNativeUIAbiCurrentConnectionRejectsV1_0
        current/connection_rejection_test.cpp
        support/host_loader.c)
    target_include_directories(
        ERNativeUIAbiCurrentConnectionRejectsV1_0 PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(
        ERNativeUIAbiCurrentConnectionRejectsV1_0 PRIVATE
        cxx_std_17)
    set_target_properties(
        ERNativeUIAbiCurrentConnectionRejectsV1_0 PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES)
    target_compile_definitions(
        ERNativeUIAbiCurrentConnectionRejectsV1_0 PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(
            ERNativeUIAbiCurrentConnectionRejectsV1_0 PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()

    add_library(ERNativeUIAbiConnectionHost SHARED
        support/connection_host.cpp)
    target_include_directories(ERNativeUIAbiConnectionHost PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(ERNativeUIAbiConnectionHost PRIVATE cxx_std_17)
    target_compile_definitions(ERNativeUIAbiConnectionHost PRIVATE
        ERUI_HOST_EXPORTS WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(ERNativeUIAbiConnectionHost PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()
    set_target_properties(ERNativeUIAbiConnectionHost PROPERTIES
        OUTPUT_NAME ERNativeUI
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_BINARY_DIR}/abi/runtime/connection")

    add_executable(ERNativeUIAbiCurrentConnectionContract
        current/connection_contract_test.cpp
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiCurrentConnectionContract PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(ERNativeUIAbiCurrentConnectionContract PRIVATE
        cxx_std_17)
    set_target_properties(ERNativeUIAbiCurrentConnectionContract PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES)
    target_compile_definitions(ERNativeUIAbiCurrentConnectionContract PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(ERNativeUIAbiCurrentConnectionContract PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()
    add_dependencies(ERNativeUIAbiCurrentConnectionContract
        ERNativeUIAbiConnectionHost)
    add_test(NAME ERNativeUI.ABI.Connection.CurrentExplicit
        COMMAND ERNativeUIAbiCurrentConnectionContract
            "$<TARGET_FILE:ERNativeUIAbiConnectionHost>")

    add_executable(ERNativeUIAbiDescriptorGuardCurrent
        current/descriptor_guard_test.cpp
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiDescriptorGuardCurrent PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(ERNativeUIAbiDescriptorGuardCurrent PRIVATE
        cxx_std_17)
    set_target_properties(ERNativeUIAbiDescriptorGuardCurrent PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES)
    target_compile_definitions(ERNativeUIAbiDescriptorGuardCurrent PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(ERNativeUIAbiDescriptorGuardCurrent PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()

    add_executable(ERNativeUIAbiCurrentTextInputContract
        current/text_input_contract_test.c
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiCurrentTextInputContract PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    set_target_properties(ERNativeUIAbiCurrentTextInputContract PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    target_compile_definitions(ERNativeUIAbiCurrentTextInputContract PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)

    add_executable(ERNativeUIAbiCurrentColorPickerContract
        current/color_picker_contract_test.c
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiCurrentColorPickerContract PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    set_target_properties(ERNativeUIAbiCurrentColorPickerContract PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    target_compile_definitions(ERNativeUIAbiCurrentColorPickerContract PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)

    add_executable(ERNativeUIAbiCurrentColorPickerWrapper
        current/color_picker_wrapper_test.cpp
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiCurrentColorPickerWrapper PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(ERNativeUIAbiCurrentColorPickerWrapper PRIVATE
        cxx_std_17)
    set_target_properties(ERNativeUIAbiCurrentColorPickerWrapper PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES)
    target_compile_definitions(ERNativeUIAbiCurrentColorPickerWrapper PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(ERNativeUIAbiCurrentColorPickerWrapper PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()

    add_executable(ERNativeUIAbiCurrentBuiltinPagesContract
        current/builtin_pages_contract_test.c
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiCurrentBuiltinPagesContract PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    set_target_properties(ERNativeUIAbiCurrentBuiltinPagesContract PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    target_compile_definitions(ERNativeUIAbiCurrentBuiltinPagesContract PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)

    add_executable(ERNativeUIAbiCurrentBuiltinPagesWrapper
        current/builtin_pages_wrapper_test.cpp
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiCurrentBuiltinPagesWrapper PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(ERNativeUIAbiCurrentBuiltinPagesWrapper PRIVATE
        cxx_std_17)
    set_target_properties(ERNativeUIAbiCurrentBuiltinPagesWrapper PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES)
    target_compile_definitions(ERNativeUIAbiCurrentBuiltinPagesWrapper PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(ERNativeUIAbiCurrentBuiltinPagesWrapper PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()

    add_executable(ERNativeUIAbiCurrentInputBindingsContract
        current/input_bindings_contract_test.c
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiCurrentInputBindingsContract PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    set_target_properties(ERNativeUIAbiCurrentInputBindingsContract PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    target_compile_definitions(ERNativeUIAbiCurrentInputBindingsContract PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)

    add_executable(ERNativeUIAbiCurrentInputBindingsWrapper
        current/input_bindings_wrapper_test.cpp
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiCurrentInputBindingsWrapper PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(ERNativeUIAbiCurrentInputBindingsWrapper PRIVATE
        cxx_std_17)
    set_target_properties(ERNativeUIAbiCurrentInputBindingsWrapper PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES)
    target_compile_definitions(ERNativeUIAbiCurrentInputBindingsWrapper PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(ERNativeUIAbiCurrentInputBindingsWrapper PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()
    add_dependencies(ERNativeUIAbiCurrentInputBindingsWrapper
        ERNativeUIAbiConnectionHost)
    add_test(NAME ERNativeUI.ABI.InputBindings.CurrentWrapper
        COMMAND ERNativeUIAbiCurrentInputBindingsWrapper
            "$<TARGET_FILE:ERNativeUIAbiConnectionHost>")

    add_executable(ERNativeUIAbiCurrentStorageContract
        current/storage_contract_test.c
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiCurrentStorageContract PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    set_target_properties(ERNativeUIAbiCurrentStorageContract PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    target_compile_definitions(ERNativeUIAbiCurrentStorageContract PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)

    if(TARGET ERNativeUIAbiHostCurrent)
        set(_erui_v1_0_include
            "${CMAKE_CURRENT_LIST_DIR}/releases/v1_0/include")

        add_executable(ERNativeUIAbiV1_0ToCurrentC
            releases/v1_0/current_host_smoke_test.c
            support/host_loader.c)
        target_include_directories(ERNativeUIAbiV1_0ToCurrentC BEFORE PRIVATE
            "${_erui_v1_0_include}"
            "${ERNATIVEUI_ABI_SUPPORT_DIR}")
        set_target_properties(ERNativeUIAbiV1_0ToCurrentC PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED YES
            C_EXTENSIONS NO)
        target_compile_definitions(ERNativeUIAbiV1_0ToCurrentC PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX)

        add_executable(ERNativeUIAbiV1_0ToCurrentCpp
            releases/v1_0/current_host_wrapper_smoke_test.cpp
            support/host_loader.c)
        target_include_directories(
            ERNativeUIAbiV1_0ToCurrentCpp BEFORE PRIVATE
            "${_erui_v1_0_include}"
            "${ERNATIVEUI_ABI_SUPPORT_DIR}")
        target_compile_features(ERNativeUIAbiV1_0ToCurrentCpp PRIVATE
            cxx_std_17)
        set_target_properties(ERNativeUIAbiV1_0ToCurrentCpp PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED YES)
        target_compile_definitions(ERNativeUIAbiV1_0ToCurrentCpp PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX)
        if(MSVC)
            target_compile_options(ERNativeUIAbiV1_0ToCurrentCpp PRIVATE
                /EHsc /Zc:__cplusplus)
        endif()

        if(TARGET ERNativeUIAbiHostV1_0Pending)
            add_executable(ERNativeUIAbiV1_0PendingReadinessLifecycle
                releases/v1_0/pending_readiness_lifecycle_test.cpp
                support/host_loader.c)
            target_include_directories(
                ERNativeUIAbiV1_0PendingReadinessLifecycle BEFORE PRIVATE
                "${_erui_v1_0_include}"
                "${ERNATIVEUI_ABI_SUPPORT_DIR}")
            target_compile_features(
                ERNativeUIAbiV1_0PendingReadinessLifecycle PRIVATE
                cxx_std_17)
            set_target_properties(
                ERNativeUIAbiV1_0PendingReadinessLifecycle PROPERTIES
                C_STANDARD 11
                C_STANDARD_REQUIRED YES)
            target_compile_definitions(
                ERNativeUIAbiV1_0PendingReadinessLifecycle PRIVATE
                WIN32_LEAN_AND_MEAN NOMINMAX)
            if(MSVC)
                target_compile_options(
                    ERNativeUIAbiV1_0PendingReadinessLifecycle PRIVATE
                    /EHsc /Zc:__cplusplus)
            endif()
            add_dependencies(
                ERNativeUIAbiV1_0PendingReadinessLifecycle
                ERNativeUIAbiHostV1_0Pending)
            add_test(
                NAME ERNativeUI.ABI.Compat.V1_0.PendingReadinessLifecycle
                COMMAND ERNativeUIAbiV1_0PendingReadinessLifecycle
                    "$<TARGET_FILE:ERNativeUIAbiHostV1_0Pending>")
            set_tests_properties(
                ERNativeUI.ABI.Compat.V1_0.PendingReadinessLifecycle
                PROPERTIES TIMEOUT 10)
        endif()

        add_test(NAME ERNativeUI.ABI.Compat.V1_0.CToCurrent
            COMMAND ERNativeUIAbiV1_0ToCurrentC
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
        add_test(NAME ERNativeUI.ABI.Compat.V1_0.CppToCurrent
            COMMAND ERNativeUIAbiV1_0ToCurrentCpp
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
        add_test(NAME ERNativeUI.ABI.Negotiation.CurrentProduction
            COMMAND ERNativeUIAbiNegotiationCurrent
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>" current)
        add_test(NAME ERNativeUI.ABI.GuardedProvider.CurrentProduction
            COMMAND ERNativeUIAbiDescriptorGuardCurrent
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
        add_test(NAME ERNativeUI.ABI.TextInput.CurrentProductionC
            COMMAND ERNativeUIAbiCurrentTextInputContract
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
        add_test(NAME ERNativeUI.ABI.ColorPicker.CurrentProductionC
            COMMAND ERNativeUIAbiCurrentColorPickerContract
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
        add_test(NAME ERNativeUI.ABI.ColorPicker.CurrentProductionCpp
            COMMAND ERNativeUIAbiCurrentColorPickerWrapper
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
        add_test(NAME ERNativeUI.ABI.BuiltinPages.CurrentProductionC
            COMMAND ERNativeUIAbiCurrentBuiltinPagesContract
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
        add_test(NAME ERNativeUI.ABI.BuiltinPages.CurrentProductionCpp
            COMMAND ERNativeUIAbiCurrentBuiltinPagesWrapper
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
        add_test(NAME ERNativeUI.ABI.InputBindings.CurrentProductionC
            COMMAND ERNativeUIAbiCurrentInputBindingsContract
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
        add_test(NAME ERNativeUI.ABI.Storage.CurrentProductionC
            COMMAND ERNativeUIAbiCurrentStorageContract
                "$<TARGET_FILE:ERNativeUIAbiHostCurrent>")
    endif()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/AddAbiRelease.cmake")
add_subdirectory(releases)
