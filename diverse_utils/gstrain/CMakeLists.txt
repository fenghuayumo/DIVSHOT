
option(TIME_TRACE "Build with -ftime-trace (clang only)" OFF)

# Project definition
project(gstrain)

# Library directories (must be set before add_library)
if(WIN32)
    link_directories(
        ${TORCH_LIB_DIR}
        ${OPENCV_LIB_DIR}
        ${WKS_LOCATION}/external/pkg/liblink
    )
endif()

# Create static library
add_library(gstrain SHARED)

# C++ standard`
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
file(GLOB_RECURSE SOURCES 
    "src/*.h"
    "src/*.c"
    "src/*.cpp"
)

target_sources(gstrain PRIVATE ${SOURCES})

# Organize source files into folders in Visual Studio
if(MSVC)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" 
                 PREFIX "Source Files" 
                 FILES ${SOURCES})
endif()

# Include directories
target_include_directories(gstrain PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# External include directories
target_include_directories(gstrain PUBLIC
    ${COLMAP_INCLUDE_DIR}
    ${STB_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
    ${CEREAL_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${DIVERSE_INCLUDE_DIR}
    ${DIVERSE_BASE_INCLUDE_DIR}
    ${OPENCV_INCLUDE_DIR}
    ${TINYGSPLAT_INCLUDE_DIR}
    ${METALIR_INCLUDE_DIR}
    ${NANOBIND_INCLUDE_DIR}
    ${PYBIND11_INCLUDE_DIR}
    ${PY3_INCLUDE_DIR}
    ${GSPLATRAST_INCLUDE_DIR}
    ${TORCH_INCLUDE_DIRS}
    ${WKS_LOCATION}/external/pkg/include/
)

# Link libraries
target_link_libraries(gstrain PUBLIC
    spdlog
    stbimage
    tinyply
    tinygsplat
    diverse_base
    gsplatrast
    pugixml
    ${TORCH_LIBRARIES}
    ${OpenCV_LIBS}
)

# Link FFmpeg libraries if available
if(FFMPEG_FOUND)
    target_include_directories(gstrain PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(gstrain PUBLIC ${FFMPEG_LIBRARIES})
    target_link_directories(gstrain PUBLIC ${FFMPEG_LIBRARY_DIRS})
endif()


# Defines
target_compile_definitions(gstrain PUBLIC
    DS_ENGINE
    DS_DYNAMIC
    FREEIMAGE_LIB
    "IMGUI_USER_CONFIG=\"imgui/ImUserConfig.h\""
    SPDLOG_COMPILED_LIB
    GLM_FORCE_INTRINSICS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_SWIZZLE
    SPLAT_EDIT
)

# OpenMP
# find_package(OpenMP REQUIRED)
# target_link_libraries(gstrain PUBLIC OpenMP::OpenMP_CXX)

# Time trace option
if(TIME_TRACE)
    target_compile_options(gstrain PRIVATE -ftime-trace)
    target_link_options(gstrain PRIVATE -ftime-trace)
endif()

# Architecture specific
# if(CMAKE_SIZEOF_VOID_P EQUAL 8)
#     target_compile_definitions(gstrain PUBLIC USE_VMA_ALLOCATOR)
# endif()

# Platform specific settings
if(WIN32)
    target_compile_definitions(gstrain PUBLIC
        DS_PLATFORM_WINDOWS
        DS_RENDER_API_OPENGL
        DS_RENDER_API_VULKAN
        VK_USE_PLATFORM_WIN32_KHR
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _DISABLE_EXTENDED_ALIGNED_STORAGE
        _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
        _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
        DS_IMGUI
        DS_OPENAL
        DS_VOLK
        DS_USE_GLFW_WINDOWS
        USE_VMA_ALLOCATOR
        USE_CUDA
    )

    target_link_libraries(gstrain PUBLIC
        Dbghelp
        Dwmapi
    )

    target_compile_options(gstrain PRIVATE /bigobj /FS)
    
elseif(APPLE)
    if(IOS)
        # iOS specific settings
        target_compile_definitions(gstrain PUBLIC
            DS_PLATFORM_IOS
            DS_PLATFORM_MOBILE
            DS_PLATFORM_UNIX
            DS_RENDER_API_VULKAN
            VK_USE_PLATFORM_IOS_MVK
            DS_IMGUI
            USE_VMA_ALLOCATOR
            DS_OPENAL
            USE_MPS
        )
           
    else()
        # macOS specific settings
        target_compile_definitions(gstrain PUBLIC
            DS_PLATFORM_MACOS
            DS_PLATFORM_UNIX
            DS_RENDER_API_OPENGL
            DS_RENDER_API_VULKAN
            # DS_RENDER_API_METAL
            DS_RENDER_API_WGPU
            VK_USE_PLATFORM_METAL_EXT
            DS_IMGUI
            DS_OPENAL
            DS_VOLK
            USE_VMA_ALLOCATOR
            USE_MPS
        )
    endif()
    # Common Apple frameworks
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    find_library(OPENAL_FRAMEWORK OpenAL REQUIRED)
    
    target_link_libraries(gstrain PUBLIC
        ${QUARTZCORE_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        ${OPENAL_FRAMEWORK}
        ${COLMAP_LIB_DIR}/libcolmap_dvs.dylib
    )

    target_compile_options(diverse PRIVATE
        -Wno-attributes
        -Wno-nullability-completeness
        -fdiagnostics-absolute-paths
    )
    add_custom_command(
        # copy     ${COLMAP_LIB_DIR}/libcolmap_dvs.dylib to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcolmap_dvs.dylib
        TARGET gstrain POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${COLMAP_LIB_DIR}/libcolmap_dvs.dylib $<TARGET_FILE_DIR:gstrain>/libcolmap_dvs.dylib
        COMMAND ${CMAKE_COMMAND} -E copy ${WKS_LOCATION}/external/pkg/liblink/libmesh_utils.dylib $<TARGET_FILE_DIR:gstrain>/libmesh_utils.dylib
        COMMENT "copy  colmap  lib to bin"
    )

elseif(UNIX)
    # Linux specific settings
    target_compile_definitions(diverse PUBLIC
        DS_PLATFORM_LINUX
        DS_PLATFORM_UNIX
        DS_RENDER_API_OPENGL
        DS_RENDER_API_VULKAN
        VK_USE_PLATFORM_XCB_KHR
        DS_IMGUI
        DS_VOLK
        DS_OPENAL
        USE_CUDA
    )
    target_link_libraries(gstrain PUBLIC
        X11
        pthread
        ${COLMAP_LIB_DIR}/libcolmap_dvs.so
    )

    target_compile_options(gstrain PRIVATE
        -fpermissive
        -fPIC
        -Wignored-attributes
        -Wno-psabi
    )

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        target_compile_options(gstrain PRIVATE -msse4.1)
    endif()
    add_custom_command(
        # copy     ${COLMAP_LIB_DIR}/libcolmap_dvs.so to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcolmap_dvs.so
        TARGET gstrain POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${COLMAP_LIB_DIR}/libcolmap_dvs.so ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcolmap_dvs.so
        COMMENT "copy  colmap  lib to bin"
    )
    # set_target_properties(gstrain PROPERTIES
    #     LINK_FLAGS "-Wl,-rpath=\\$$ORIGIN"
    # )
endif()

# 设置不同的构建类型对应的编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug 配置
    add_compile_definitions(
        DS_DEBUG 
        _DEBUG
        TRACY_ENABLE
        DS_PROFILE_ENABLED
        TRACY_ON_DEMAND
    )
    # 启用调试符号
    set(CMAKE_DEBUG_POSTFIX "d")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
    # 禁用优化
    if(MSVC)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od")
    else()
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
    endif()

elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Release 配置
    add_compile_definitions(
        DS_RELEASE
        NDEBUG
        TRACY_ENABLE
        DS_PROFILE_ENABLED
        TRACY_ON_DEMAND
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

# 设置运行时库（仅Windows MSVC）
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    endif()
endif()