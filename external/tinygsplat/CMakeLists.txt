cmake_minimum_required(VERSION 3.10)
project(tinygsplat)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create static library
add_library(tinygsplat SHARED
    tiny_gsplat.hpp
    tiny_gsplat.cpp
)

# Include directories
target_include_directories(tinygsplat PUBLIC
    ${GLM_INCLUDE_DIR}
    ${TINYPLY_INCLUDE_DIR}
    ${WKS_LOCATION}/external/spz/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/tinygsplat
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_link_libraries(tinygsplat PRIVATE 
    ZLIB::ZLIB
    spz,
    tinyply
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