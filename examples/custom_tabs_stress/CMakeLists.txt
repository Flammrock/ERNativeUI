cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(CustomTabsStress LANGUAGES CXX)
    # This in-tree build uses the current API 1.4 wrapper while exercising the
    # custom-tab surface introduced in API 1.3. Omitting a package version here
    # avoids confusing the SDK release with the negotiated runtime API.
    find_package(ERNativeUI CONFIG REQUIRED)
    set(ERNATIVEUI_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH
        "Directory in which the example DLL is staged")
endif()

add_library(CustomTabsStress MODULE custom_tabs_stress.cpp)
target_link_libraries(CustomTabsStress PRIVATE ERNativeUI::SDK)
target_compile_features(CustomTabsStress PRIVATE cxx_std_17)
target_compile_definitions(CustomTabsStress PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
set_target_properties(CustomTabsStress PROPERTIES PREFIX "")
if(MSVC)
    target_compile_options(CustomTabsStress PRIVATE
        /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()
add_custom_command(TARGET CustomTabsStress POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:CustomTabsStress>"
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples/$<TARGET_FILE_NAME:CustomTabsStress>"
    VERBATIM)
