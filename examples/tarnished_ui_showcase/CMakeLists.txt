cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(TarnishedUIShowcase LANGUAGES CXX)
    find_package(ERNativeUI 1.1.0 CONFIG REQUIRED)
    set(ERNATIVEUI_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH
        "Directory in which the example DLL is staged")
endif()

add_library(TarnishedUIShowcase MODULE showcase.cpp)
target_link_libraries(TarnishedUIShowcase PRIVATE ERNativeUI::SDK)
target_compile_features(TarnishedUIShowcase PRIVATE cxx_std_17)
target_compile_definitions(TarnishedUIShowcase PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(TarnishedUIShowcase PROPERTIES PREFIX "")
if(MSVC)
    target_compile_options(TarnishedUIShowcase PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()
add_custom_command(TARGET TarnishedUIShowcase POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:TarnishedUIShowcase>"
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples/$<TARGET_FILE_NAME:TarnishedUIShowcase>"
    VERBATIM)
