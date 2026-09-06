cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MyERNativeUIMod LANGUAGES CXX)
    find_package(ERNativeUI 1.1.0 CONFIG REQUIRED)
    set(ERNATIVEUI_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH
        "Directory in which the example DLL is staged")
endif()

# This target links only the header-only SDK target. It does not link to an
# ERNativeUI import library, so the mod DLL can still load when the host is
# absent and report that its optional settings menu is unavailable.
add_library(MyERNativeUIMod MODULE mod_main.cpp)
target_link_libraries(MyERNativeUIMod PRIVATE ERNativeUI::SDK)
target_compile_features(MyERNativeUIMod PRIVATE cxx_std_17)
target_compile_definitions(MyERNativeUIMod PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(MyERNativeUIMod PROPERTIES PREFIX "")
if(MSVC)
    target_compile_options(MyERNativeUIMod PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()
add_custom_command(TARGET MyERNativeUIMod POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:MyERNativeUIMod>"
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples/$<TARGET_FILE_NAME:MyERNativeUIMod>"
    VERBATIM)
