cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ERNativeUIExamples LANGUAGES CXX)
    find_package(ERNativeUI 1.1.0 CONFIG REQUIRED)
    set(ERNATIVEUI_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH
        "Directory in which example DLLs are staged")
endif()

add_subdirectory(tarnished_ui_showcase)
add_subdirectory(localized_greeting)
add_subdirectory(showcase_screenshot_helper)
add_subdirectory(template)
