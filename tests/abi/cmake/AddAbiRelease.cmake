include_guard(GLOBAL)

function(erui_add_abi_release release_token)
    set(release_dir
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../releases/${release_token}")
    if(NOT EXISTS "${release_dir}/release.cmake")
        message(FATAL_ERROR
            "Unknown ERNativeUI ABI release '${release_token}'.")
    endif()

    unset(ERUI_ABI_RELEASE_TOKEN)
    unset(ERUI_ABI_RELEASE_API_VERSION)
    unset(ERUI_ABI_RELEASE_API_PREFIX_SIZE)
    unset(ERUI_ABI_RELEASE_C_STANDARD)
    unset(ERUI_ABI_RELEASE_CXX_STANDARD)
    unset(ERUI_ABI_RELEASE_ERUI_SHA256)
    unset(ERUI_ABI_RELEASE_CPP_SHA256)
    include("${release_dir}/release.cmake")

    if(NOT ERUI_ABI_RELEASE_TOKEN STREQUAL release_token)
        message(FATAL_ERROR
            "ABI release metadata token mismatch for '${release_token}'.")
    endif()

    set(release_include "${release_dir}/include")
    set(c_header "${release_include}/ernativeui/erui.h")
    set(cpp_header "${release_include}/ernativeui/ERNativeUI.hpp")
    foreach(required IN ITEMS "${c_header}" "${cpp_header}")
        if(NOT EXISTS "${required}")
            message(FATAL_ERROR
                "Frozen ABI release is incomplete: ${required}")
        endif()
    endforeach()

    file(SHA256 "${c_header}" actual_c_sha256)
    file(SHA256 "${cpp_header}" actual_cpp_sha256)
    string(TOLOWER "${actual_c_sha256}" actual_c_sha256)
    string(TOLOWER "${actual_cpp_sha256}" actual_cpp_sha256)
    if(NOT actual_c_sha256 STREQUAL ERUI_ABI_RELEASE_ERUI_SHA256)
        message(FATAL_ERROR
            "Frozen ${release_token} erui.h changed. Expected "
            "${ERUI_ABI_RELEASE_ERUI_SHA256}, got ${actual_c_sha256}.")
    endif()
    if(NOT actual_cpp_sha256 STREQUAL ERUI_ABI_RELEASE_CPP_SHA256)
        message(FATAL_ERROR
            "Frozen ${release_token} ERNativeUI.hpp changed. Expected "
            "${ERUI_ABI_RELEASE_CPP_SHA256}, got ${actual_cpp_sha256}.")
    endif()

    string(TOUPPER "${release_token}" release_upper)
    set(layout_target "ERNativeUIAbiLayout${release_upper}")
    add_executable(${layout_target} "${release_dir}/layout_test.c")
    target_include_directories(${layout_target} BEFORE PRIVATE
        "${release_include}")
    set_target_properties(${layout_target} PROPERTIES
        C_STANDARD ${ERUI_ABI_RELEASE_C_STANDARD}
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    add_test(NAME "ERNativeUI.ABI.Release.${release_upper}.Layout"
        COMMAND ${layout_target})

    if(NOT WIN32)
        return()
    endif()

    set(contract_host "ERNativeUIAbiHost${release_upper}Contract")
    add_library(${contract_host} SHARED
        "${release_dir}/contract_host.cpp")
    target_include_directories(${contract_host} BEFORE PRIVATE
        "${release_include}"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(${contract_host} PRIVATE
        "cxx_std_${ERUI_ABI_RELEASE_CXX_STANDARD}")
    target_compile_definitions(${contract_host} PRIVATE
        ERUI_HOST_EXPORTS
        ERUI_COMPAT_HOST_EXPORTS
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(${contract_host} PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()
    set_target_properties(${contract_host} PROPERTIES
        OUTPUT_NAME ERNativeUI
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_BINARY_DIR}/abi/runtime/${release_token}")

    set(c_client "ERNativeUIAbiClient${release_upper}C")
    add_executable(${c_client}
        "${release_dir}/c_client_test.c"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}/host_loader.c")
    target_include_directories(${c_client} BEFORE PRIVATE
        "${release_include}"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    set_target_properties(${c_client} PROPERTIES
        C_STANDARD ${ERUI_ABI_RELEASE_C_STANDARD}
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
    target_compile_definitions(${c_client} PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)

    set(cpp_client "ERNativeUIAbiClient${release_upper}Cpp")
    add_executable(${cpp_client}
        "${release_dir}/cpp_client_test.cpp"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}/host_loader.c")
    target_include_directories(${cpp_client} BEFORE PRIVATE
        "${release_include}"
        "${ERNATIVEUI_ABI_SUPPORT_DIR}")
    target_compile_features(${cpp_client} PRIVATE
        "cxx_std_${ERUI_ABI_RELEASE_CXX_STANDARD}")
    set_target_properties(${cpp_client} PROPERTIES
        C_STANDARD ${ERUI_ABI_RELEASE_C_STANDARD}
        C_STANDARD_REQUIRED YES)
    target_compile_definitions(${cpp_client} PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(${cpp_client} PRIVATE
            /EHsc /Zc:__cplusplus)
    endif()

    add_test(NAME "ERNativeUI.ABI.Contract.${release_upper}.C"
        COMMAND ${c_client} "$<TARGET_FILE:${contract_host}>")
    add_test(NAME "ERNativeUI.ABI.Contract.${release_upper}.Cpp"
        COMMAND ${cpp_client} "$<TARGET_FILE:${contract_host}>")

    if(TARGET ERNativeUIAbiNegotiationCurrent)
        add_test(NAME
            "ERNativeUI.ABI.Compat.CurrentTo${release_upper}.Negotiation"
            COMMAND ERNativeUIAbiNegotiationCurrent
                "$<TARGET_FILE:${contract_host}>" old-host)
    endif()
    if(TARGET ERNativeUIAbiCurrentWrapperFallback)
        add_test(NAME
            "ERNativeUI.ABI.Compat.CurrentTo${release_upper}.Wrapper"
            COMMAND ERNativeUIAbiCurrentWrapperFallback
                "$<TARGET_FILE:${contract_host}>")
    endif()
    if(TARGET ERNativeUIAbiDescriptorGuardCurrent)
        add_test(NAME
            "ERNativeUI.ABI.Compat.CurrentTo${release_upper}.GuardedProvider"
            COMMAND ERNativeUIAbiDescriptorGuardCurrent
                "$<TARGET_FILE:${contract_host}>")
    endif()

    if(DEFINED ERNATIVEUI_ABI_CURRENT_HOST_TARGET AND
       TARGET ${ERNATIVEUI_ABI_CURRENT_HOST_TARGET})
        add_test(NAME "ERNativeUI.ABI.Compat.${release_upper}.CToCurrent"
            COMMAND ${c_client}
                "$<TARGET_FILE:${ERNATIVEUI_ABI_CURRENT_HOST_TARGET}>")
        add_test(NAME "ERNativeUI.ABI.Compat.${release_upper}.CppToCurrent"
            COMMAND ${cpp_client}
                "$<TARGET_FILE:${ERNATIVEUI_ABI_CURRENT_HOST_TARGET}>")
        if(TARGET ERNativeUIAbiNegotiationCurrent)
            add_test(NAME "ERNativeUI.ABI.Negotiation.Current"
                COMMAND ERNativeUIAbiNegotiationCurrent
                    "$<TARGET_FILE:${ERNATIVEUI_ABI_CURRENT_HOST_TARGET}>"
                    current)
        endif()
        if(TARGET ERNativeUIAbiDescriptorGuardCurrent)
            add_test(NAME "ERNativeUI.ABI.GuardedProvider.Current"
                COMMAND ERNativeUIAbiDescriptorGuardCurrent
                    "$<TARGET_FILE:${ERNATIVEUI_ABI_CURRENT_HOST_TARGET}>")
        endif()
    endif()
endfunction()
