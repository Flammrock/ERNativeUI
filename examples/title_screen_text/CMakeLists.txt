cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(TitleScreenTextExample LANGUAGES CXX)
    # Runtime API negotiation is independent from the SDK package's release
    # version. The source requires the API 1.3 declarations in the headers.
    find_package(ERNativeUI CONFIG REQUIRED)
    set(ERNATIVEUI_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH
        "Directory in which the example DLL is staged")
endif()

add_library(TitleScreenTextExample MODULE title_screen_text.cpp)
target_link_libraries(TitleScreenTextExample PRIVATE ERNativeUI::SDK)
target_compile_features(TitleScreenTextExample PRIVATE cxx_std_17)
target_compile_definitions(TitleScreenTextExample PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
set_target_properties(TitleScreenTextExample PROPERTIES PREFIX "")
if(MSVC)
    target_compile_options(TitleScreenTextExample PRIVATE
        /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()
add_custom_command(TARGET TitleScreenTextExample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:TitleScreenTextExample>"
        "${ERNATIVEUI_DEPLOY_DIR}/$<CONFIG>/examples/$<TARGET_FILE_NAME:TitleScreenTextExample>"
    VERBATIM)
