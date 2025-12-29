cmake_minimum_required(VERSION 3.18)
project(TinySOG VERSION 0.1.0 LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Force C++17 for CUDA to avoid compiler internal errors
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -std=c++17")

# CUDA 架构策略:
# - Production: 编译多架构以兼容不同用户显卡
# - Release/Debug: 只编译当前主机显卡架构以加快编译速度
if(CMAKE_BUILD_TYPE STREQUAL "Production")
    SET(CMAKE_CUDA_ARCHITECTURES 70;80;89;120 CACHE STRING "CUDA architectures" FORCE)
    message(STATUS "Production build - tinysog CUDA architectures: 70;80;120")
else()
    SET(CMAKE_CUDA_ARCHITECTURES native CACHE STRING "CUDA architectures" FORCE)
    message(STATUS "Development build - tinysog CUDA architectures: native")
endif()

# Clear Torch added CUDA arch flags and remove /MP flag (force sm_70 and sm_80 only)
if(CMAKE_CUDA_FLAGS)
    # Remove -gencode and arch=compute_xxx,code=sm_xxx flags
    string(REGEX REPLACE "-gencode [^ ]+" "" CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS}")
    string(REGEX REPLACE "arch=compute_[0-9]+a?,code=sm_[0-9]+a?" "" CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS}")
    # Remove /MP flag (not supported by nvcc)
    string(REGEX REPLACE "/MP" "" CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS}")
    message(STATUS "Cleared Torch CUDA arch flags and /MP for tinysog, CMAKE_CUDA_FLAGS: ${CMAKE_CUDA_FLAGS}")
endif()

# 查找依赖
find_package(CUDAToolkit REQUIRED)

# Use nlohmann-json (from vcpkg or system)
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    # Fallback to project's json if available
    set(JSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../json)
endif()

# Check for LibArchive and WebP - required for tinysog
# User should install these via vcpkg or system package manager
# When using vcpkg, manually set LibArchive paths
if(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    # Detect vcpkg root from toolchain file path
    get_filename_component(VCPKG_ROOT "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
    get_filename_component(VCPKG_ROOT "${VCPKG_ROOT}" DIRECTORY)
    get_filename_component(VCPKG_ROOT "${VCPKG_ROOT}" DIRECTORY)
    
    # Set LibArchive paths for vcpkg
    if(NOT VCPKG_TARGET_TRIPLET)
        if(WIN32)
            set(VCPKG_TARGET_TRIPLET "x64-windows")
        elseif(APPLE)
            set(VCPKG_TARGET_TRIPLET "x64-osx")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-linux")
        endif()
    endif()
    
    set(LibArchive_INCLUDE_DIR "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include" CACHE PATH "LibArchive include directory")
    set(LibArchive_LIBRARY "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib/archive.lib" CACHE FILEPATH "LibArchive library")
    message(STATUS "Using vcpkg LibArchive: ${LibArchive_LIBRARY}")
endif()
find_package(LibArchive REQUIRED)
find_package(WebP REQUIRED CONFIG)

# Try to find tl-expected, or use tinysog's bundled version
find_package(tl-expected CONFIG QUIET)
if(NOT tl-expected_FOUND)
    message(STATUS "tl-expected not found, using tinysog's bundled expected.h")
endif()

# TinySOG库
add_library(tinysog
    # 公共API
    src/tinysog.cpp

    # 内部实现
    src/internal/data_types.cpp
    src/internal/webp_utils.cpp
    src/internal/archive_utils.cpp

    # CUDA kernels
    src/internal/decoder.cu
    src/internal/morton.cu
    src/internal/kmeans.cu
    src/internal/encoder.cu
)

# Set CUDA source files language property (same as gsplatrast)
file(GLOB_RECURSE TINYSOG_CUDA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/*.cu")
set_source_files_properties(${TINYSOG_CUDA_SOURCES} PROPERTIES 
    LANGUAGE CUDA
    VS_NO_COMPILE_BATCHING ON
)

target_include_directories(tinysog
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add JSON include dir only if using bundled version
if(NOT nlohmann_json_FOUND AND JSON_INCLUDE_DIR)
    target_include_directories(tinysog PRIVATE ${JSON_INCLUDE_DIR})
endif()

# Handle tl-expected: add vcpkg include directory if using vcpkg
if(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    get_filename_component(VCPKG_ROOT "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
    get_filename_component(VCPKG_ROOT "${VCPKG_ROOT}" DIRECTORY)
    get_filename_component(VCPKG_ROOT "${VCPKG_ROOT}" DIRECTORY)
    set(VCPKG_INSTALLED_DIR "${VCPKG_ROOT}/installed/x64-windows")
    if(EXISTS "${VCPKG_INSTALLED_DIR}/include")
        target_include_directories(tinysog PRIVATE "${VCPKG_INSTALLED_DIR}/include")
        message(STATUS "Added vcpkg include directory: ${VCPKG_INSTALLED_DIR}/include")
    endif()
endif()

target_link_libraries(tinysog
    PUBLIC
        CUDA::cudart
        CUDA::cuda_driver
    PRIVATE
        LibArchive::LibArchive
        WebP::webp
)

# Link tl::expected if found, otherwise use bundled version
if(tl-expected_FOUND)
    target_link_libraries(tinysog PUBLIC tl::expected)
endif()

# Link nlohmann_json if found
if(nlohmann_json_FOUND)
    target_link_libraries(tinysog PRIVATE nlohmann_json::nlohmann_json)
endif()

# 编译选项
target_compile_options(tinysog PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:
        --use_fast_math
        --expt-relaxed-constexpr
        --extended-lambda
        -lineinfo
    >
)

# Disable Visual Studio parallel compilation for CUDA (nvcc doesn't support /MP)
set_target_properties(tinysog PROPERTIES
    CUDA_SEPARABLE_COMPILATION OFF
    VS_NO_COMPILE_BATCHING ON
)

# Remove /MP from CUDA compilation options (nvcc doesn't support it)
get_target_property(CUDA_OPTIONS tinysog COMPILE_OPTIONS)
if(CUDA_OPTIONS)
    list(REMOVE_ITEM CUDA_OPTIONS /MP)
    set_target_properties(tinysog PROPERTIES COMPILE_OPTIONS "${CUDA_OPTIONS}")
endif()

# 安装规则
include(GNUInstallDirs)

install(TARGETS tinysog
    EXPORT TinySOGTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 导出配置
install(EXPORT TinySOGTargets
    FILE TinySOGTargets.cmake
    NAMESPACE TinySOG::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TinySOG
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/TinySOGConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/TinySOGConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TinySOG
)

# ========================================
# Examples and Tools (Disabled by default)
# ========================================
# Disable examples and tools by default to avoid additional dependencies
option(TINYSOG_BUILD_EXAMPLES "Build TinySOG examples" OFF)
option(TINYSOG_BUILD_TOOLS "Build TinySOG tools" OFF)

if(TINYSOG_BUILD_EXAMPLES)
    add_executable(tinysog_example examples/example.cpp)
    target_link_libraries(tinysog_example PRIVATE tinysog)
    
    add_executable(tinysog_roundtrip_test examples/roundtrip_test.cpp)
    target_link_libraries(tinysog_roundtrip_test PRIVATE tinysog)
    
    message(STATUS "TinySOG examples will be built")
endif()

if(TINYSOG_BUILD_TOOLS)
    add_executable(ply_to_sog tools/ply_to_sog.cpp)
    target_link_libraries(ply_to_sog PRIVATE tinysog)
    
    add_executable(sog_to_ply tools/sog_to_ply.cpp)
    target_link_libraries(sog_to_ply PRIVATE tinysog)
    
    add_executable(verify_sog tools/verify_sog.cpp)
    target_link_libraries(verify_sog PRIVATE tinysog)
    
    add_executable(dump_webp_data tools/dump_webp_data.cpp)
    target_link_libraries(dump_webp_data PRIVATE LibArchive::LibArchive WebP::webp)
    
    add_executable(deep_verify tools/deep_verify.cpp)
    target_link_libraries(deep_verify PRIVATE tinysog)
    
    message(STATUS "TinySOG tools will be built")
endif()
