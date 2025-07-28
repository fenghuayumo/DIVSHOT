cmake_minimum_required(VERSION 3.16)
project(ozz_animation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ozz_base library
add_library(ozz_base STATIC)
file(GLOB_RECURSE CPP_FILES  
"${CMAKE_CURRENT_SOURCE_DIR}/include/ozz/base/**.h"
"${CMAKE_CURRENT_SOURCE_DIR}/src/base/*.cc"
)
target_sources(ozz_base PRIVATE ${CPP_FILES})
target_include_directories(ozz_base
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ozz_animation library
add_library(ozz_animation STATIC)
file(GLOB_RECURSE ANIM_CPP_FILES  
"${CMAKE_CURRENT_SOURCE_DIR}/include/ozz/animation/runtime/**.h"
"${CMAKE_CURRENT_SOURCE_DIR}/src/animation/runtime/*.h"
"${CMAKE_CURRENT_SOURCE_DIR}/src/animation/runtime/*.cc"
)
target_sources(ozz_animation PRIVATE ${ANIM_CPP_FILES})

target_include_directories(ozz_animation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ozz_animation_offline library
add_library(ozz_animation_offline STATIC)

file(GLOB ANIMOFFLINE_CPP_FILES
"${CMAKE_CURRENT_SOURCE_DIR}/include/ozz/animation/offline/*.h" 
"${CMAKE_CURRENT_SOURCE_DIR}/src/animation/offline/*.h" 
"${CMAKE_CURRENT_SOURCE_DIR}/src/animation/offline/*.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/src/animation/offline/tools/*.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/src/options/*.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/extern/jsoncpp/dist/jsoncpp.cpp"
)
target_sources(ozz_animation_offline PRIVATE ${ANIMOFFLINE_CPP_FILES})

target_include_directories(ozz_animation_offline
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/jsoncpp/dist
)

# Set common compiler options based on configuration
foreach(TARGET ozz_base ozz_animation ozz_animation_offline)
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE
            $<$<CONFIG:Debug>:${CMAKE_CXX_FLAGS_DEBUG}>
            $<$<CONFIG:Release>:${CMAKE_CXX_FLAGS_RELEASE}>
            $<$<CONFIG:Production>:-o3>
        )
    endif()
endforeach()