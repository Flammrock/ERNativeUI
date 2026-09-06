cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ShowcaseScreenshotHelper LANGUAGES CXX)
    find_package(ERNativeUI 1.1.0 CONFIG REQUIRED)
    set(ERNATIVEUI_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH
        "Directory in which the example DLL is staged")
endif()

add_library(ShowcaseScreenshotHelper MODULE showcase_screenshot_helper.cpp)
target_link_libraries(ShowcaseScreenshotHelper PRIVATE ERNativeUI::SDK)
target_compile_features(ShowcaseScreenshotHelper PRIVATE cxx_std_17)
target_compile_definitions(ShowcaseScreenshotHelper PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(ShowcaseScreenshotHelper PROPERTIES PREFIX "")
if(MSVC)
    target_compile_options(ShowcaseScreenshotHelper PRIVATE
        /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()
add_custom_target(ShowcaseScreenshotHelperStage ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ShowcaseScreenshotHelper>"
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples/$<TARGET_FILE_NAME:ShowcaseScreenshotHelper>"
    COMMAND ${CMAKE_COMMAND}
        "-DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/ShowcaseScreenshotHelper.ini"
        "-DOUTPUT_FILE=${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples/ShowcaseScreenshotHelper.ini"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/copy_if_missing.cmake"
    DEPENDS ShowcaseScreenshotHelper
    VERBATIM)
