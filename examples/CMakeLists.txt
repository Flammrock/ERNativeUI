cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ERNativeUIExamples LANGUAGES CXX)
    # The in-tree examples use the current API 1.4 wrapper and also demonstrate
    # features introduced by earlier APIs. Package-release numbers are
    # independent from API versions; the imported headers select the surface.
    find_package(ERNativeUI CONFIG REQUIRED)
    set(ERNATIVEUI_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH
        "Directory in which example DLLs are staged")
endif()

add_subdirectory(tarnished_ui_showcase)
add_subdirectory(localized_greeting)
add_subdirectory(showcase_screenshot_helper)
add_subdirectory(template)
add_subdirectory(custom_tabs_stress)
add_subdirectory(title_screen_text)
