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

    add_executable(ERNativeUIAbiCurrentWrapperFallback
        current/wrapper_fallback_test.cpp
        support/host_loader.c)
    target_include_directories(ERNativeUIAbiCurrentWrapperFallback PRIVATE
        "${ERNATIVEUI_ABI_REPOSITORY_ROOT}/include"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(ERNativeUIAbiCurrentWrapperFallback PRIVATE
        cxx_std_17)
    set_target_properties(ERNativeUIAbiCurrentWrapperFallback PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES)
    target_compile_definitions(ERNativeUIAbiCurrentWrapperFallback PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(ERNativeUIAbiCurrentWrapperFallback PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()

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
    endif()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/AddAbiRelease.cmake")
add_subdirectory(releases)
