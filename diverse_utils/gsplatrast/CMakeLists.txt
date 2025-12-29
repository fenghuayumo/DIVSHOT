
option(TIME_TRACE "Build with -ftime-trace (clang only)" OFF)

# Project definition
project(gsplatrast)

# 清除 Torch 添加的所有 CUDA 架构标志，并移除 /MP 标志（强制只用 sm_70 和 sm_80,sm89）
if(CMAKE_CUDA_FLAGS)
    # 删除 -gencode 和 arch=compute_xxx,code=sm_xxx 标志
    string(REGEX REPLACE "-gencode [^ ]+" "" CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS}")
    string(REGEX REPLACE "arch=compute_[0-9]+a?,code=sm_[0-9]+a?" "" CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS}")
    # 删除 /MP 标志 (nvcc 不支持)
    string(REGEX REPLACE "/MP" "" CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS}")
    message(STATUS "Cleared Torch CUDA arch flags and /MP, CMAKE_CUDA_FLAGS: ${CMAKE_CUDA_FLAGS}")
endif()

# C++ standard`
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(GPU_RUNTIME STREQUAL "CUDA")
    find_package(CUDAToolkit)
    if (NOT CUDAToolkit_FOUND)
        message(WARNING "CUDA toolkit not found, building with CPU support only")
        set(GPU_RUNTIME "CPU")
    else()
        # CUDA 架构策略:
        # - Production: 编译多架构以兼容不同用户显卡 (70, 80, 89, 120)
        # - Release/Debug: 只编译当前主机显卡架构以加快编译速度
        if(CMAKE_BUILD_TYPE STREQUAL "Production")
            SET(CMAKE_CUDA_ARCHITECTURES 70;80;89;120 CACHE STRING "CUDA architectures" FORCE)
            message(STATUS "Production build - CUDA architectures: 70;80;89;120 (multi-GPU compatibility)")
        else()
            # 使用 native 让 CMake 自动检测当前主机 GPU 架构
            SET(CMAKE_CUDA_ARCHITECTURES native CACHE STRING "CUDA architectures" FORCE)
            message(STATUS "Development build - CUDA architectures: native (auto-detect host GPU)")
        endif()
        
        file(GLOB_RECURSE GSPLAT_GPU_SRC LIST_DIRECTORIES False src/rasterizer/gsplat-cuda/*.cu src/rasterizer/gsplat-cuda/*.cpp)
        set_source_files_properties(${GSPLAT_GPU_SRC} PROPERTIES LANGUAGE CUDA)
    endif()
elseif(GPU_RUNTIME STREQUAL "HIP")
    set(USE_HIP ON CACHE BOOL "Use HIP for GPU acceleration")

    if(NOT DEFINED HIP_PATH)
        if(NOT DEFINED ENV{HIP_PATH})
            set(HIP_PATH "/opt/rocm/hip" CACHE PATH "Path to which HIP has been installed")
        else()
            set(HIP_PATH $ENV{HIP_PATH} CACHE PATH "Path to which HIP has been installed")
        endif()
    endif()
    set(CMAKE_MODULE_PATH "${HIP_PATH}/cmake" ${CMAKE_MODULE_PATH})
    find_package(HIP REQUIRED)

    file(GLOB_RECURSE GSPLAT_GPU_SRC LIST_DIRECTORIES False src/rasterizer/gsplat-cuda/*.cu)
    set_source_files_properties(${GSPLAT_GPU_SRC} PROPERTIES LANGUAGE HIP)

    if(WIN32)
        set(ROCM_ROOT "$ENV{HIP_PATH}" CACHE PATH "Root directory of the ROCm installation")
    else()
        set(ROCM_ROOT "/opt/rocm" CACHE PATH "Root directory of the ROCm installation")
    endif()
    list(APPEND CMAKE_PREFIX_PATH "${ROCM_ROOT}")
elseif(GPU_RUNTIME STREQUAL "MPS")
    find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
    find_library(METAL_FRAMEWORK    Metal      REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit   REQUIRED)
    message(STATUS "Metal framework found")
    file(GLOB_RECURSE GSPLAT_GPU_SRC LIST_DIRECTORIES False src/rasterizer/gsplat-metal/*.mm)
    set(XC_FLAGS -O3)
    set(USE_MPS ON CACHE BOOL "Use MPS for GPU acceleration")
else()
    set(GPU_RUNTIME "CPU")
endif()

add_library(gsplatrast STATIC)
if(GPU_RUNTIME STREQUAL "HIP")
    target_compile_definitions(gsplatrast PRIVATE USE_HIP __HIP_PLATFORM_AMD__)
    target_sources(gsplatrast PRIVATE ${GSPLAT_GPU_SRC})
elseif(GPU_RUNTIME STREQUAL "CUDA")
    target_compile_definitions(gsplatrast PRIVATE USE_CUDA)
    target_sources(gsplatrast PRIVATE ${GSPLAT_GPU_SRC})
    
    # Disable separable compilation
    set_target_properties(gsplatrast PROPERTIES 
        CUDA_SEPARABLE_COMPILATION OFF
        CUDA_RESOLVE_DEVICE_SYMBOLS OFF
        # Disable parallel compilation for CUDA (MSVC /MP flag breaks nvcc)
        VS_NO_COMPILE_BATCHING ON
    )
    
    # Remove /MP from CUDA compilation options (nvcc doesn't support it)
    get_target_property(CUDA_OPTIONS gsplatrast COMPILE_OPTIONS)
    if(CUDA_OPTIONS)
        list(REMOVE_ITEM CUDA_OPTIONS /MP)
        set_target_properties(gsplatrast PROPERTIES COMPILE_OPTIONS "${CUDA_OPTIONS}")
    endif()
    
    # Explicitly set CUDA compile options without /MP
    target_compile_options(gsplatrast PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler="/EHsc /W3 /nologo /O2 /FS /MD /GR">
    )
elseif(GPU_RUNTIME STREQUAL "MPS")
    target_compile_definitions(gsplatrast PRIVATE USE_MPS)
    target_sources(gsplatrast PRIVATE ${GSPLAT_GPU_SRC} src/rasterizer/gsplat-cpu/gsplat_cpu.cpp)
    # copy shader files to bin directory
    configure_file(src/rasterizer/gsplat-metal/gsplat_metal.metal ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/gsplat_metal.metal COPYONLY)
    add_custom_command(
        OUTPUT ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/default.metallib
        COMMAND xcrun -sdk macosx metal    ${XC_FLAGS} -c ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/gsplat_metal.metal -o ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/gsplat_metal.air
        
        COMMAND xcrun -sdk macosx metallib                ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/gsplat_metal.air   -o $<TARGET_FILE_DIR:gsplatrast>/default.metallib
        COMMAND rm -f ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/gsplat_metal.air
        COMMAND rm -f ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/gsplat_metal.metal
        DEPENDS src/rasterizer/gsplat-metal/gsplat_metal.metal
        COMMENT "Compiling Metal kernels"
    )

    add_custom_target(
        gsplat_metal ALL
        DEPENDS ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/default.metallib
    )
endif()

# Include directories
target_include_directories(gsplatrast PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# External include directories
target_include_directories(gsplatrast PUBLIC
    ${ENTT_INCLUDE_DIR}
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
    ${TORCH_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(gsplatrast PUBLIC
    spdlog
    stbimage
    tinyply
    tinygsplat
    diverse_base
    ${TORCH_LIBRARIES}
)

# Defines
target_compile_definitions(gsplatrast PUBLIC
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
    target_compile_options(gsplatrast PRIVATE -ftime-trace)
    target_link_options(gsplatrast PRIVATE -ftime-trace)
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

    target_link_libraries(gsplatrast PUBLIC
        Dbghelp
        Dwmapi
    )

    # Add /bigobj only for C/C++ files, not CUDA
    target_compile_options(gsplatrast PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/bigobj>
    )
    
elseif(APPLE)
    if(IOS)
        # iOS specific settings
        target_compile_definitions(gsplatrast PUBLIC
            DS_PLATFORM_IOS
            DS_PLATFORM_MOBILE
            DS_PLATFORM_UNIX
            DS_RENDER_API_VULKAN
            VK_USE_PLATFORM_IOS_MVK
            DS_IMGUI
            USE_VMA_ALLOCATOR
            DS_OPENAL
        )
           
    else()
        # macOS specific settings
        target_compile_definitions(gsplatrast PUBLIC
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
    endif()

    # Common Apple frameworks
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit   REQUIRED)
    target_link_libraries(gsplatrast PUBLIC
        ${QUARTZCORE_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${FOUNDATION_LIBRARY}
        ${METALKIT_FRAMEWORK}
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
    target_link_libraries(gsplatrast PUBLIC
        X11
        pthread
    )

    target_compile_options(gsplatrast PRIVATE
        -fpermissive
        -fPIC
        -Wignored-attributes
        -Wno-psabi
    )

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        target_compile_options(gsplatrast PRIVATE -msse4.1)
    endif()

    # set_target_properties(gsplatrast PROPERTIES
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
