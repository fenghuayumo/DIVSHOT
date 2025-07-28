cmake_minimum_required(VERSION 3.16)
project(imgui)

# Create static library
add_library(imgui STATIC
    imconfig.h
    imgui.h
    imgui.cpp
    imgui_draw.cpp
    imgui_internal.h
    imgui_widgets.cpp
    imgui_tables.cpp
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
    imgui_demo.cpp
    Plugins/ImGuizmo.h
    Plugins/ImGuizmo.cpp
    Plugins/ImTextEditor.h
    Plugins/ImTextEditor.cpp
    Plugins/imcmd_command_palette.cpp
    Plugins/imcmd_fuzzy_search.cpp
    Plugins/implot/implot.h
    Plugins/implot/implot.cpp
    Plugins/implot/implot_internal.h
    Plugins/implot/implot_items.cpp
)

# Set C++17
target_compile_features(imgui PRIVATE cxx_std_17)

# Include directories
target_include_directories(imgui PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

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
    add_definitions(
        -DDS_RELEASE 
        -DNDEBUG 
    )
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
    
elseif(CMAKE_BUILD_TYPE STREQUAL "Production")
add_definitions(
    -DDS_PRODUCTION 
    -DNDEBUG
)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_PRODUCTION} -O3")
endif()