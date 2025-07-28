include(CMakePrintHelpers)
cmake_print_variables(CPACK_TEMPORARY_DIRECTORY)
cmake_print_variables(CPACK_TOPLEVEL_DIRECTORY)
cmake_print_variables(CPACK_PACKAGE_DIRECTORY)
cmake_print_variables(CPACK_PACKAGE_FILE_NAME)
cmake_print_variables(CMAKE_SYSTEM_PROCESSOR)
cmake_print_variables(CPACK_INSTALL_CMAKE_PROJECTS)

set(TEV_VERSION "@SPLATX_VERSION@")
list(GET CPACK_INSTALL_CMAKE_PROJECTS 0 SPLATX_BUILD_DIR)

file(SHA256 "${CPACK_PACKAGE_FILES}" SPLATX_INSTALLER_SHA256)
configure_file("${SPLATX_BUILD_DIR}/resources/winget.yaml" "${SPLATX_BUILD_DIR}/splatx.yaml")
