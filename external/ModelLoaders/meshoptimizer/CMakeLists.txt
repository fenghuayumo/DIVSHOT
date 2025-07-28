cmake_minimum_required(VERSION 3.10)
project(meshoptimizer)

# Create static library
add_library(${PROJECT_NAME} STATIC)

# Add source files
file(GLOB SOURCES "src/*.cpp" "src/*.h")
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})

# Configure build settings for different configurations
set_target_properties(${PROJECT_NAME} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

# Debug configuration
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:-g>             # Debug symbols
)

# Release and Production configurations
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:Production>>:-O3>  # Optimization level
)