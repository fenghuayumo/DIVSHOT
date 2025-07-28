cmake_minimum_required(VERSION 3.10)
project(stbimage)

# Create static library
add_library(stbimage STATIC
    image_utils.h
    image_utils.cpp
)

# Set C++17 standard
target_compile_features(stbimage PRIVATE cxx_std_17)

# Include directories
target_include_directories(stbimage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    set(CMAKE_SYSTEM_VERSION ${CMAKE_SYSTEM_VERSION})
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    set_property(TARGET stbimage PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

# Configuration-specific settings
set_target_properties(stbimage PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

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