cmake_minimum_required(VERSION 3.10)
project(msdf-atlas-gen)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable warnings
if(MSVC)
    add_compile_options(/w)
else()
    add_compile_options(-w)
endif()

# Add source files
file(GLOB_RECURSE MSDF_ATLAS_SOURCES 
    "msdf-atlas-gen/*.h"
    "msdf-atlas-gen/*.hpp"
    "msdf-atlas-gen/*.cpp"
)

file(GLOB_RECURSE MSDFGEN_SOURCES
    "msdfgen/core/*.h"
    "msdfgen/core/*.hpp"
    "msdfgen/core/*.cpp"
    "msdfgen/ext/*.h"
    "msdfgen/ext/*.hpp"
    "msdfgen/ext/*.cpp"
    "msdfgen/lib/*.cpp"
    "msdfgen/include/*.h"
)

# Remove main.cpp from sources
list(FILTER MSDF_ATLAS_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

# Create static library
add_library(${PROJECT_NAME} STATIC ${MSDF_ATLAS_SOURCES} ${MSDFGEN_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC 
    "msdf-atlas-gen"
    "msdfgen/include"
    "msdfgen"
    "../freetype/include"
)

# Definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    _CRT_SECURE_NO_WARNINGS
    MSDFGEN_USE_CPP11
)

# Link freetype
# find_package(Freetype REQUIRED)
# target_link_libraries(${PROJECT_NAME} PRIVATE Freetype::Freetype)
target_link_libraries(${PROJECT_NAME} PRIVATE freetype)

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