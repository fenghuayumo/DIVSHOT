cmake_minimum_required(VERSION 3.10)
project(tinygsplat)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create shared library
add_library(tinygsplat SHARED
    tiny_gsplat.hpp
    tiny_gsplat.cpp
)

# Define export macros for shared library
target_compile_definitions(tinygsplat PRIVATE GS_ENGINE GS_DYNAMIC)

# Include directories
target_include_directories(tinygsplat PUBLIC
    ${GLM_INCLUDE_DIR}
    ${TINYPLY_INCLUDE_DIR}
    ${WKS_LOCATION}/external/spz/include/
    ${WKS_LOCATION}/external/tinysog/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/tinygsplat
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# Add vcpkg include directory if using vcpkg (for tl-expected)
if(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    get_filename_component(VCPKG_ROOT "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
    get_filename_component(VCPKG_ROOT "${VCPKG_ROOT}" DIRECTORY)
    get_filename_component(VCPKG_ROOT "${VCPKG_ROOT}" DIRECTORY)
    if(NOT VCPKG_TARGET_TRIPLET)
        if(WIN32)
            set(VCPKG_TARGET_TRIPLET "x64-windows")
        elseif(APPLE)
            set(VCPKG_TARGET_TRIPLET "x64-osx")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-linux")
        endif()
    endif()
    target_include_directories(tinygsplat PRIVATE "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include")
endif()
# Link tinysog if available (must be first for dependency order)
if(TARGET tinysog)
    target_link_libraries(tinygsplat PUBLIC tinysog)
    target_compile_definitions(tinygsplat PRIVATE HAS_TINYSOG)
endif()

target_link_libraries(tinygsplat PRIVATE 
    spz
    tinyply
    ${CMAKE_CURRENT_SOURCE_DIR}/../pkg/liblink/zlib.lib
)
# Platform-specific settings
if(WIN32)
    # Windows-specific settings if needed
elseif(UNIX)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Configuration-specific settings
set(CMAKE_DEBUG_POSTFIX "d")


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(
        -DDS_DEBUG 
        -D_DEBUG 
    )
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
    
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(
        -DDS_RELEASE 
        -DNDEBUG 
    )
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    
elseif(CMAKE_BUILD_TYPE STREQUAL "Production")
add_definitions(
    -DDS_PRODUCTION 
    -DNDEBUG
)
set(CMAKE_CXX_FLAGS_PRODUCTION "${CMAKE_CXX_FLAGS_PRODUCTION} -O3")
endif()