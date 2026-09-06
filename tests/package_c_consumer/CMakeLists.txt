cmake_minimum_required(VERSION 3.24)

project(ERNativeUICPackageConsumer LANGUAGES C)

find_package(ERNativeUI 1.0 CONFIG REQUIRED)

add_executable(ERNativeUICPackageConsumer main.c)
target_link_libraries(ERNativeUICPackageConsumer PRIVATE ERNativeUI::CABI)
set_target_properties(ERNativeUICPackageConsumer PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO)
