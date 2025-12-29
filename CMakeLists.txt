cmake_minimum_required(VERSION 3.13)
project(
    diverse
    VERSION 0.6.1
    DESCRIPTION "lightweight neural renderer."
    LANGUAGES C CXX
)
set(DIVERSE_VERSION "${CMAKE_PROJECT_VERSION}")

if (NOT DIVERSE_DEPLOY)
    set(DIVERSE_VERSION "${DIVERSE_VERSION}dev")
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DIVERSE_VERSION_ARCH "${DIVERSE_VERSION} (64 bit)")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(DIVERSE_VERSION_ARCH "${DIVERSE_VERSION} (32 bit)")
endif()


# Add Production configuration type
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Production" CACHE STRING "Available build configurations" FORCE)

# Set default build type if not specified (for single-config generators like Makefile)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug/Release/Production)" FORCE)
endif()

# Define Production build flags (start from Release but remove debug symbols)
if(MSVC)
    # Start with Release flags but remove /Zi (debug symbols) for Production
    string(REPLACE "/Zi" "" CMAKE_CXX_FLAGS_PRODUCTION_BASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/Zi" "" CMAKE_C_FLAGS_PRODUCTION_BASE "${CMAKE_C_FLAGS_RELEASE}")
    set(CMAKE_CXX_FLAGS_PRODUCTION "${CMAKE_CXX_FLAGS_PRODUCTION_BASE}" CACHE STRING "Flags used by the C++ compiler during Production builds." FORCE)
    set(CMAKE_C_FLAGS_PRODUCTION "${CMAKE_C_FLAGS_PRODUCTION_BASE}" CACHE STRING "Flags used by the C compiler during Production builds." FORCE)
else()
    # Start with Release flags but remove -g (debug symbols) for Production
    string(REPLACE "-g" "" CMAKE_CXX_FLAGS_PRODUCTION_BASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "-g" "" CMAKE_C_FLAGS_PRODUCTION_BASE "${CMAKE_C_FLAGS_RELEASE}")
    set(CMAKE_CXX_FLAGS_PRODUCTION "${CMAKE_CXX_FLAGS_PRODUCTION_BASE}" CACHE STRING "Flags used by the C++ compiler during Production builds." FORCE)
    set(CMAKE_C_FLAGS_PRODUCTION "${CMAKE_C_FLAGS_PRODUCTION_BASE}" CACHE STRING "Flags used by the C compiler during Production builds." FORCE)
endif()
set(CMAKE_EXE_LINKER_FLAGS_PRODUCTION "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING "Flags used by the linker during Production builds." FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_PRODUCTION "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}" CACHE STRING "Flags used by the linker during Production builds." FORCE)

# Production optimizations (Full optimization like Premake)
if(MSVC)
    # /Ox: Full optimization (includes /Ob2 /Oi /Ot /Oy /GF /Gy) - matches Premake's optimize "Full"
    # /DDS_PRODUCTION: Production build macro
    # /DNDEBUG: Disable assertions
    set(CMAKE_CXX_FLAGS_PRODUCTION "${CMAKE_CXX_FLAGS_PRODUCTION} /Ox /DNDEBUG /DDS_PRODUCTION")
    set(CMAKE_C_FLAGS_PRODUCTION "${CMAKE_C_FLAGS_PRODUCTION} /Ox /DNDEBUG /DDS_PRODUCTION")
else()
    # -O3: Highest optimization level - matches Premake's optimize "Full"
    # -DDS_PRODUCTION: Production build macro
    # -DNDEBUG: Disable assertions
    set(CMAKE_CXX_FLAGS_PRODUCTION "${CMAKE_CXX_FLAGS_PRODUCTION} -O3 -DNDEBUG -DDS_PRODUCTION")
    set(CMAKE_C_FLAGS_PRODUCTION "${CMAKE_C_FLAGS_PRODUCTION} -O3 -DNDEBUG -DDS_PRODUCTION")
endif()

mark_as_advanced(
    CMAKE_CXX_FLAGS_PRODUCTION
    CMAKE_C_FLAGS_PRODUCTION
    CMAKE_EXE_LINKER_FLAGS_PRODUCTION
    CMAKE_SHARED_LINKER_FLAGS_PRODUCTION
)

# Architecture handling
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
    set(ARCH "arm")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(ARCH "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86")
    set(ARCH "x86")
endif()

# Set GPU_RUNTIME based on platform
if(APPLE)
    set(GPU_RUNTIME "MPS" CACHE STRING "HIP or CUDA or MPS")
elseif(WIN32 OR UNIX)
    set(GPU_RUNTIME "CUDA" CACHE STRING "HIP or CUDA or MPS")
else()
    set(GPU_RUNTIME "CPU" CACHE STRING "HIP or CUDA or MPS")
endif()
set(GSPLAT_MAX_CUDA_COMPATIBILITY OFF CACHE BOOL "Build for maximum CUDA device compatibility")
# Output directories
# 设置输出目录为 build/<config> (例如 build/Release, build/Debug)
# 对于多配置生成器（Visual Studio），使用配置名作为子目录
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    string(TOLOWER ${CONFIG_TYPE} CONFIG_TYPE_LOWER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/${CONFIG_TYPE_LOWER})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/${CONFIG_TYPE_LOWER})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/${CONFIG_TYPE_LOWER})
endforeach()

# 对于单配置生成器（Makefile, Ninja）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})

# Enable multi-processor compilation
if(MSVC)
    add_compile_options(/MP)
endif()
# CUDA support for Windows and Linux
if(WIN32 OR UNIX AND NOT APPLE)
    find_package(CUDA REQUIRED)
    message(STATUS "CUDA Version: ${CUDA_VERSION}")
endif()

if (APPLE)
    set(CMAKE_MACOSX_RPATH ON)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Environment variables
set(VK_SDK_PATH $ENV{VK_SDK_PATH})
set(VULKAN_SDK $ENV{VULKAN_SDK})
set(LIBTORCH_SDK $ENV{LIBTORCH_SDK})
set(CUDA_PATH $ENV{CUDA_PATH})
set(PY3_PATH $ENV{PY3_PATH})
set(OPEN3D_PATH $ENV{OPEN3D_PATH})

# Auto-detect Vulkan SDK from environment variables
# Priority: VK_SDK_PATH > VULKAN_SDK (if both are set, VK_SDK_PATH takes precedence)
if(NOT DEFINED Vulkan_INCLUDE_DIR AND NOT DEFINED Vulkan_LIBRARY)
    # Use VK_SDK_PATH first if available
    if(VK_SDK_PATH)
        set(VULKAN_SDK ${VK_SDK_PATH})
        message(STATUS "Using VK_SDK_PATH: ${VK_SDK_PATH}")
        # Override VULKAN_SDK environment variable for find_package(Vulkan)
        set(ENV{VULKAN_SDK} ${VK_SDK_PATH})
    elseif(VULKAN_SDK)
        message(STATUS "Using VULKAN_SDK: ${VULKAN_SDK}")
    endif()
    
    if(VULKAN_SDK)
        if(WIN32)
            set(Vulkan_INCLUDE_DIR "${VULKAN_SDK}/Include")
            set(Vulkan_LIBRARY "${VULKAN_SDK}/Lib/vulkan-1.lib")
        elseif(APPLE)
            set(Vulkan_INCLUDE_DIR "${VULKAN_SDK}/include")
            set(Vulkan_LIBRARY "${VULKAN_SDK}/lib/libvulkan.dylib")
        else()
            set(Vulkan_INCLUDE_DIR "${VULKAN_SDK}/include")
            set(Vulkan_LIBRARY "${VULKAN_SDK}/lib/libvulkan.so")
        endif()
        message(STATUS "Auto-configured Vulkan SDK: ${VULKAN_SDK}")
        message(STATUS "  Include: ${Vulkan_INCLUDE_DIR}")
        message(STATUS "  Library: ${Vulkan_LIBRARY}")
    endif()
endif()

find_package(Vulkan)
# Define workspace location variable
set(WKS_LOCATION ${CMAKE_CURRENT_SOURCE_DIR})

set(ENTT_INCLUDE_DIR ${WKS_LOCATION}/external/entt/src)
set(GLFW_INCLUDE_DIR ${WKS_LOCATION}/external/glfw/include)
set(GLAD_INCLUDE_DIR ${WKS_LOCATION}/external/glad/include)
set(LUA_INCLUDE_DIR ${WKS_LOCATION}/external/lua/src)
set(STB_INCLUDE_DIR ${WKS_LOCATION}/external/stb)
set(IMGUI_INCLUDE_DIR ${WKS_LOCATION}/external/imgui)
set(OPENAL_INCLUDE_DIR ${WKS_LOCATION}/external/OpenAL/include)
set(BOX2D_INCLUDE_DIR ${WKS_LOCATION}/external/box2d/include)
# set(VULKAN_INCLUDE_DIR ${WKS_LOCATION}/external/vulkan)
set(VULKAN_INCLUDE_DIR ${${Vulkan_INCLUDE_DIRS}})
set(EXTERNAL_INCLUDE_DIR ${WKS_LOCATION}/external)
set(SPDLOG_INCLUDE_DIR ${WKS_LOCATION}/external/spdlog/include)
set(FREETYPE_INCLUDE_DIR ${WKS_LOCATION}/external/freetype/include)
set(SPIRV_CROSS_INCLUDE_DIR ${WKS_LOCATION}/external/vulkan/SPIRV-Cross)
set(CEREAL_INCLUDE_DIR ${WKS_LOCATION}/external/cereal/include)
set(GLM_INCLUDE_DIR ${WKS_LOCATION}/external/glm)
set(MSDFGEN_INCLUDE_DIR ${WKS_LOCATION}/external/msdf-atlas-gen/msdfgen)
set(MSDF_ATLAS_GEN_INCLUDE_DIR ${WKS_LOCATION}/external/msdf-atlas-gen/msdf-atlas-gen)
set(DIVERSE_INCLUDE_DIR ${WKS_LOCATION}/diverse/source)
set(DIVERSE_BASE_INCLUDE_DIR ${WKS_LOCATION}/diverse_base/source)
set(CLI_INCLUDE_DIR ${WKS_LOCATION}/external/CLI11/include/)
set(TINYGSPLAT_INCLUDE_DIR ${WKS_LOCATION}/external/tinygsplat)
set(TINYPLY_INCLUDE_DIR ${WKS_LOCATION}/external/tinyply)
set(METALIR_INCLUDE_DIR ${WKS_LOCATION}/external/metal_irconverter_runtime/)
set(NANOBIND_INCLUDE_DIR ${WKS_LOCATION}/external/nanobind)
set(PYBIND11_INCLUDE_DIR ${WKS_LOCATION}/external/pybind11/include)
set(OZZ_INCLUDE_DIR ${WKS_LOCATION}/external/ozz-animation/include)
set(WEBGPU_INCLUDE_DIR ${WKS_LOCATION}/external/webgpu/include)
set(COLMAP_INCLUDE_DIR ${WKS_LOCATION}/external/colmap/include)
set(PUGIXML_INCLUDE_DIR ${WKS_LOCATION}/external/pugixml/src)
set(RAPIDCSV_INCLUDE_DIR ${WKS_LOCATION}/external/rapidcsv/)
set(GSTRAIN_INCLUDE_DIR ${WKS_LOCATION}/diverse_utils/gstrain/src)
set(GSPLATRAST_INCLUDE_DIR ${WKS_LOCATION}/diverse_utils/gsplatrast/src)
set(PY3_INCLUDE_DIR ${PY3_PATH}/include)
set(CUDA_INCLUDE_DIR ${CUDA_PATH}/include)

# Auto-detect Torch from PY3_PATH environment variable
if(NOT DEFINED Torch_DIR AND PY3_PATH)
    if(WIN32)
        set(Torch_DIR "${PY3_PATH}/Lib/site-packages/torch/share/cmake/Torch")
    elseif(APPLE)
        # macOS Homebrew Python or system Python
        if(EXISTS "${PY3_PATH}/lib/python3.12/site-packages/torch/share/cmake/Torch")
            set(Torch_DIR "${PY3_PATH}/lib/python3.12/site-packages/torch/share/cmake/Torch")
        elseif(EXISTS "/opt/homebrew/opt/pytorch")
            set(Torch_DIR "/opt/homebrew/opt/pytorch")
        endif()
    else()
        # Linux
        set(Torch_DIR "${PY3_PATH}/lib/python3/site-packages/torch/share/cmake/Torch")
    endif()
    
    if(Torch_DIR AND EXISTS ${Torch_DIR})
        message(STATUS "Auto-configured Torch from PY3_PATH: ${Torch_DIR}")
    endif()
endif()

# Find OpenCV first to avoid protobuf conflicts with Torch
find_package(OpenCV CONFIG REQUIRED)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")

# Force OpenCV to always use Release libraries for all configurations
# This prevents linking against debug libraries (opencv_*d.lib)
foreach(opencv_target ${OpenCV_LIBS})
    if(TARGET ${opencv_target})
        set_target_properties(${opencv_target} PROPERTIES
            MAP_IMPORTED_CONFIG_DEBUG Release
            MAP_IMPORTED_CONFIG_RELEASE Release
            MAP_IMPORTED_CONFIG_PRODUCTION Release
        )
    endif()
endforeach()
message(STATUS "OpenCV configured to use Release libraries for all build configurations")

# Find Torch after OpenCV
find_package(Torch REQUIRED)

# Find FFmpeg components
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG REQUIRED 
        libavcodec 
        libavformat 
        libswscale
        libavutil
    )
    message(STATUS "FFmpeg found via pkg-config")
    message(STATUS "  avcodec: ${FFMPEG_libavcodec_VERSION}")
    message(STATUS "  avformat: ${FFMPEG_libavformat_VERSION}")
    message(STATUS "  swscale: ${FFMPEG_libswscale_VERSION}")
else()
    # Fallback: try to find FFmpeg manually (vcpkg usually provides pkg-config)
    message(WARNING "pkg-config not found, FFmpeg detection may be incomplete")
endif()

find_package(ZLIB)

if (NOT WIN32 AND NOT APPLE)
    set(CMAKE_CUDA_COMPILER "${CUDA_TOOLKIT_ROOT_DIR}/bin/nvcc")
endif()

# Set OPENCV_INCLUDE_DIR for compatibility with existing code
set(OPENCV_INCLUDE_DIR ${OpenCV_INCLUDE_DIRS})

if(WIN32)
    set(LIBTORCH_INCLUDE_DIR ${LIBTORCH_SDK}/include)
    set(TORCH_LIB_DIR ${PY3_PATH}/Lib/site-packages/torch/lib)
elseif(APPLE)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external)

# Main projects
add_subdirectory(diverse/diverse_base)
add_subdirectory(diverse)
add_subdirectory(diverse_utils)
add_subdirectory(application)

# Set startup project (Visual Studio specific)
if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT divshot)
endif()

set(CPACK_PACKAGE_VENDOR "hby (BingyangHu)")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "a lightweight neural renderer.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.divshot.com/")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if (APPLE)
    install(TARGETS divshot BUNDLE DESTINATION "/Applications")
    install(SCRIPT scripts/mac-post-install.cmake)
    set(CPACK_GENERATOR "DragNDrop")
elseif (WIN32)
    install(TARGETS divshot RUNTIME DESTINATION "bin")
    set(CPACK_GENERATOR "WIX")
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-installer")

    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png")
    set(CPACK_PACKAGE_EXECUTABLES "divshot" "divshot")

    set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/patch.wxs")
    set(CPACK_WIX_UPGRADE_GUID "F3EC0D95-B2DC-4451-975C-CF591871E162")
    set(CPACK_WIX_PRODUCT_GUID "F7478E0D-33A4-4F00-829B-45CE27B1D0E2")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")

    configure_file(scripts/configure-winget.cmake scripts/configure-winget.cmake @ONLY)
    configure_file(application/resource/winget.yaml application/resource/winget.yaml COPYONLY)
    set(CPACK_POST_BUILD_SCRIPTS "${CMAKE_BINARY_DIR}/scripts/configure-winget.cmake")
else()
    install(TARGETS divshot)

    configure_file(application/resource/divshot.desktop application/resource/divshot.desktop)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/application/resource/divshot.desktop DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")

    set(ICON_RESOLUTIONS "16;24;32;48;64;96;128;256;512")
    foreach(RES IN LISTS ICON_RESOLUTIONS)
        install(FILES application/resource/icon-${RES}.png DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${RES}x${RES}/apps" RENAME divshot.png)
    endforeach()

    list(APPEND CPACK_GENERATOR External)
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/create-appimage.cmake")
    set(CPACK_EXTERNAL_ENABLE_STAGING YES)
    set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME})
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr) # requirement by AppDir specification
endif()

include(CPack)
