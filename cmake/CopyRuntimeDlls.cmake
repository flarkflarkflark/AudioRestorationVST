if(NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "DEST_DIR is required")
endif()

if(NOT DEFINED RUNTIME_DLLS OR RUNTIME_DLLS STREQUAL "")
    return()
endif()

file(MAKE_DIRECTORY "${DEST_DIR}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${RUNTIME_DLLS} "${DEST_DIR}")
