cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LocalizedGreeting LANGUAGES CXX)
    find_package(ERNativeUI 0.9 CONFIG REQUIRED)
    set(ERNATIVEUI_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH
        "Directory in which the example DLL is staged")
endif()

add_library(LocalizedGreeting MODULE localized_greeting.cpp)
target_link_libraries(LocalizedGreeting PRIVATE ERNativeUI::SDK)
target_compile_features(LocalizedGreeting PRIVATE cxx_std_17)
target_compile_definitions(LocalizedGreeting PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(LocalizedGreeting PROPERTIES PREFIX "")
if(MSVC)
    target_compile_options(LocalizedGreeting PRIVATE
        /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()
add_custom_command(TARGET LocalizedGreeting POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:LocalizedGreeting>"
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples/$<TARGET_FILE_NAME:LocalizedGreeting>"
    VERBATIM)
