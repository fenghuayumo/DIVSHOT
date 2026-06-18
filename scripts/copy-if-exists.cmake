if(NOT DEFINED src OR NOT DEFINED dst)
    message(FATAL_ERROR "copy-if-exists.cmake requires -Dsrc=<path> and -Ddst=<path>")
endif()

if(EXISTS "${src}")
    get_filename_component(dst_dir "${dst}" DIRECTORY)
    file(MAKE_DIRECTORY "${dst_dir}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${src}" "${dst}"
        RESULT_VARIABLE copy_result
    )
    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Failed to copy ${src} to ${dst}")
    endif()
endif()
