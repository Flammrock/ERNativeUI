if(NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE and OUTPUT_FILE are required")
endif()

if(NOT EXISTS "${OUTPUT_FILE}")
    get_filename_component(_output_directory "${OUTPUT_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${_output_directory}")
    file(COPY_FILE "${INPUT_FILE}" "${OUTPUT_FILE}" ONLY_IF_DIFFERENT)
endif()
