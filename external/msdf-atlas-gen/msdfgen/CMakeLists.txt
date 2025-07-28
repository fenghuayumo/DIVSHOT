cmake_minimum_required(VERSION 3.16)
project(msdfgen)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create static library
add_library(msdfgen STATIC)

# Add source files
file(GLOB_RECURSE MSDFGEN_SOURCES 
    "core/*.h"
    "core/*.hpp"
    "core/*.cpp"
    "ext/*.h"
    "ext/*.hpp"
    "ext/*.cpp"
    "lib/*.cpp"
    "include/*.h"
)
target_sources(msdfgen PRIVATE ${MSDFGEN_SOURCES})

# Include directories
target_include_directories(msdfgen PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../freetype/include"
)

# Definitions
target_compile_definitions(msdfgen PUBLIC MSDFGEN_USE_CPP11)

# Link freetype
target_link_libraries(msdfgen PUBLIC freetype)

# Disable warnings
if(MSVC)
    target_compile_options(msdfgen PRIVATE /w)
else()
    target_compile_options(msdfgen PRIVATE -w)
endif()

# Configuration-specific settings
if(MSVC)
    # Windows-specific settings
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Platform-specific settings
if(WIN32)
    target_compile_definitions(imgui PRIVATE _WINDOWS)
elseif(UNIX AND NOT APPLE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(
        -DDS_DEBUG 
        -D_DEBUG 
    )
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
    
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
add_compile_definitions(
    DS_RELEASE
    NDEBUG
)
# 启用优化但保留调试信息
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Zi")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -g")
endif()

    
elseif(CMAKE_BUILD_TYPE STREQUAL "Production")
  # Production 配置
  add_compile_definitions(
    DS_PRODUCTION
    NDEBUG
)
# 完全优化，移除调试信息
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O3")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()
endif()