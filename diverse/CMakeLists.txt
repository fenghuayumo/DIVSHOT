
option(TIME_TRACE "Build with -ftime-trace (clang only)" OFF)

# Project definition
project(diverse)

# Create static library
add_library(diverse STATIC)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
file(GLOB_RECURSE SOURCES 
    "source/*.h"
    "source/*.c"
    "source/*.cpp"
)
# Remove platform files initially - they'll be added back based on platform
list(FILTER SOURCES EXCLUDE REGEX "source/platform/.*")

target_sources(diverse PRIVATE ${SOURCES})

# Include directories
target_include_directories(diverse PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

# External include directories
target_include_directories(diverse PUBLIC
    ${ENTT_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
    ${LUA_INCLUDE_DIR}
    ${STB_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${OPENAL_INCLUDE_DIR}
    ${BOX2D_INCLUDE_DIR}
    ${VULKAN_INCLUDE_DIR}
    ${EXTERNAL_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIR}
    ${SPIRV_CROSS_INCLUDE_DIR}
    ${CEREAL_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${MSDFGEN_INCLUDE_DIR}
    ${MSDF_ATLAS_GEN_INCLUDE_DIR}
    ${DIVERSE_INCLUDE_DIR}
    ${DIVERSE_BASE_INCLUDE_DIR}
    ${OPENCV_INCLUDE_DIR}
    ${TINYGSPLAT_INCLUDE_DIR}
    ${METALIR_INCLUDE_DIR}
    ${NANOBIND_INCLUDE_DIR}
    ${PYBIND11_INCLUDE_DIR}
    ${OZZ_INCLUDE_DIR}
    ${PY3_INCLUDE_DIR}
    ${WGPU_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(diverse PUBLIC
    # lua
    # box2d
    imgui
    freetype
    # SpirvCross
    spdlog
    meshoptimizer
    msdf-atlas-gen
    ozz_base
    ozz_animation
    ozz_animation_offline
    stbimage
    tinyply
    tinygsplat
    diverse_base
    Vulkan::Vulkan
)

# Defines
target_compile_definitions(diverse PUBLIC
    DS_ENGINE
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
# target_link_libraries(diverse PUBLIC OpenMP::OpenMP_CXX)

# Time trace option
if(TIME_TRACE)
    target_compile_options(diverse PRIVATE -ftime-trace)
    target_link_options(diverse PRIVATE -ftime-trace)
endif()

# Architecture specific
# if(CMAKE_SIZEOF_VOID_P EQUAL 8)
#     target_compile_definitions(diverse PUBLIC USE_VMA_ALLOCATOR)
# endif()

# Platform specific settings
if(WIN32)
    target_compile_definitions(diverse PUBLIC
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
    )

    file(GLOB_RECURSE PLATFORM_SOURCES
        "source/platform/windows/*.h"
        "source/platform/windows/*.cpp"
        "source/platform/glfw/*.h"
        "source/platform/glfw/*.cpp"
        "../external/glad/src/glad_wgl.c"
    )
    target_sources(diverse PRIVATE ${PLATFORM_SOURCES})

    target_link_libraries(diverse PUBLIC
        Dbghelp
        glfw
        Dwmapi
    )

    target_compile_options(diverse PRIVATE /bigobj)
    
elseif(APPLE)
    if(IOS)
        # iOS specific settings
        target_compile_definitions(diverse PUBLIC
            DS_PLATFORM_IOS
            DS_PLATFORM_MOBILE
            DS_PLATFORM_UNIX
            DS_RENDER_API_VULKAN
            VK_USE_PLATFORM_IOS_MVK
            DS_IMGUI
            USE_VMA_ALLOCATOR
            DS_OPENAL
        )
        
        # iOS specific files and frameworks
        file(GLOB_RECURSE PLATFORM_SOURCES
            "source/platform/iOS/*.mm"
            "source/platform/iOS/*.h"
            "source/platform/iOS/*.cpp"
            "source/platform/openal/*.h"
            "source/platform/openal/*.cpp"
            "source/platform/unix/*.h"
            "source/platform/unix/*.cpp"
            "source/platform/vulkan/*.h"
            "source/platform/vulkan/*.cpp"
        )
        
        list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "source/platform/unix/unix_file_system.cpp")
        
    else()
        # macOS specific settings
        target_compile_definitions(diverse PUBLIC
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
        )

        # macOS specific files
        file(GLOB_RECURSE PLATFORM_SOURCES
            "source/*.mm"
            "source/platform/macOS/*.mm"
            "source/platform/macOS/*.h"
            "source/platform/macOS/*.cpp"
            "source/platform/unix/*.h"
            "source/platform/unix/*.cpp"
            "source/platform/glfw/*.h"
            "source/platform/glfw/*.cpp"
            "source/platform/openal/*.h"
            "source/platform/openal/*.cpp"
        )
    endif()

    target_sources(diverse PRIVATE ${PLATFORM_SOURCES})

    # Common Apple frameworks
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    find_library(OPENAL_FRAMEWORK OpenAL REQUIRED)
    
    target_link_libraries(diverse PUBLIC
        ${QUARTZCORE_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        ${OPENAL_FRAMEWORK}
        glfw
        # metalirconverter
    )

    target_compile_options(diverse PRIVATE
        -Wno-attributes
        -Wno-nullability-completeness
        -fdiagnostics-absolute-paths
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
    )

    file(GLOB_RECURSE PLATFORM_SOURCES
        "source/platform/unix/*.h"
        "source/platform/unix/*.cpp"
        "source/platform/glfw/*.h"
        "source/platform/glfw/*.cpp"
        "source/platform/openal/*.h"
        "source/platform/openal/*.cpp"
        "source/platform/opengl/*.h"
        "source/platform/opengl/*.cpp"
        "source/platform/vulkan/*.h"
        "source/platform/vulkan/*.cpp"
    )
    target_sources(diverse PRIVATE ${PLATFORM_SOURCES})

    target_link_libraries(diverse PUBLIC
        glfw
        openal
        X11
        pthread
    )

    target_compile_options(diverse PRIVATE
        -fpermissive
        -fPIC
        -Wignored-attributes
        -Wno-psabi
    )

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        target_compile_options(diverse PRIVATE -msse4.1)
    endif()

    # set_target_properties(diverse PROPERTIES
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