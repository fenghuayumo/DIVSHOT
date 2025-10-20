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


# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Production" CACHE STRING "Choose the type of build (Debug/Release/Production)" FORCE)
endif()

# Architecture handling
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
    set(ARCH "arm")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(ARCH "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86")
    set(ARCH "x86")
endif()

set(GPU_RUNTIME "MPS" CACHE STRING "HIP or CUDA or MPS")
set(GSPLAT_MAX_CUDA_COMPATIBILITY OFF CACHE BOOL "Build for maximum CUDA device compatibility")
# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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
set(VULKAN_SDK $ENV{VULKAN_SDK})
set(LIBTORCH_SDK $ENV{LIBTORCH_SDK})
set(CUDA_PATH $ENV{CUDA_PATH})
set(PY3_PATH $ENV{PY3_PATH})
set(OPEN3D_PATH $ENV{OPEN3D_PATH})

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
set(GSTRAIN_UTILS_INCLUDE_DIR ${WKS_LOCATION}/diverse_utils/gstrain_utils/src)
set(GSPLATRAST_INCLUDE_DIR ${WKS_LOCATION}/diverse_utils/gsplatrast/src)
set(HYBRID_INCLUDE_DIR ${WKS_LOCATION}/diverse_utils/hybrid_gstrain/src)
set(PY3_INCLUDE_DIR ${PY3_PATH}/include)

find_package(Torch REQUIRED)
find_package(OpenCV HINTS "${OPENCV_DIR}" REQUIRED)
find_package(ZLIB REQUIRED)

if (NOT WIN32 AND NOT APPLE)
    set(CMAKE_CUDA_COMPILER "${CUDA_TOOLKIT_ROOT_DIR}/bin/nvcc")
endif()


# if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/external/opencv/CMakeLists.txt")
#     set(WITH_FFMPEG ON)
# 	add_subdirectory(external/opencv)
# endif()

if(WIN32)
    set(OPENCV_INCLUDE_DIR ${WKS_LOCATION}/external/opencv4_9/include)
    set(LIBTORCH_INCLUDE_DIR ${LIBTORCH_SDK}/include)
elseif(APPLE)
    # set(OPENCV_INCLUDE_DIR ${WKS_LOCATION}/external/opencv4_9/include)
    # set(LIBTORCH_INCLUDE_DIR /opt/homebrew/Cellar/pytorch/2.2.0_3/libexec/lib/python3.12/site-packages/torch/include)
endif()
# set(OpenCV_LIBS opencv_core opencv_imgproc opencv_videoio opencv_imgcodecs)
set(OpenCV_LIBS opencv_core opencv_imgproc opencv_highgui opencv_calib3d)
if(WIN32)
set(TORCH_LIB_DIR ${PY3_PATH}/Lib/site-packages/torch/lib)
# set(OPENCV_LIB_DIR ${WKS_LOCATION}/external/opencv4_9/linklib)
set(COLMAP_LIB_DIR ${WKS_LOCATION}/external/colmap/liblink/win_cuda)
elseif(APPLE)
set(COLMAP_LIB_DIR ${WKS_LOCATION}/external/colmap/liblink/mac)
# set(OPENCV_LIB_DIR "/opt/homebrew/Cellar/opencv/4.9.0_7/lib")
# set(TORCH_LIB_DIR "/opt/homebrew/opt/pytorch")
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external)

# Main projects
add_subdirectory(diverse/diverse_base)
add_subdirectory(diverse)
add_subdirectory(diverse_utils)
add_subdirectory(application)

# Set startup project (Visual Studio specific)
if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT diverseshot)
endif()

set(CPACK_PACKAGE_VENDOR "hby (BingyangHu)")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "a lightweight neural renderer.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.diverseshot.com/")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if (APPLE)
    install(TARGETS diverseshot BUNDLE DESTINATION "/Applications")
    install(SCRIPT scripts/mac-post-install.cmake)
    set(CPACK_GENERATOR "DragNDrop")
elseif (WIN32)
    install(TARGETS diverseshot RUNTIME DESTINATION "bin")
    set(CPACK_GENERATOR "WIX")
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-installer")

    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png")
    set(CPACK_PACKAGE_EXECUTABLES "diverseshot" "diverseshot")

    set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/patch.wxs")
    set(CPACK_WIX_UPGRADE_GUID "F3EC0D95-B2DC-4451-975C-CF591871E162")
    set(CPACK_WIX_PRODUCT_GUID "F7478E0D-33A4-4F00-829B-45CE27B1D0E2")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")

    configure_file(scripts/configure-winget.cmake scripts/configure-winget.cmake @ONLY)
    configure_file(resources/winget.yaml resources/winget.yaml COPYONLY)
    set(CPACK_POST_BUILD_SCRIPTS "${CMAKE_BINARY_DIR}/scripts/configure-winget.cmake")
else()
    install(TARGETS diverseshot)

    configure_file(application/resource/diverseshot.desktop application/resource/diverseshot.desktop)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/application/resource/diverseshot.desktop DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")

    set(ICON_RESOLUTIONS "16;24;32;48;64;96;128;256;512")
    foreach(RES IN LISTS ICON_RESOLUTIONS)
        install(FILES application/resource/icon-${RES}.png DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${RES}x${RES}/apps" RENAME diverseshot.png)
    endforeach()

    list(APPEND CPACK_GENERATOR External)
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/create-appimage.cmake")
    set(CPACK_EXTERNAL_ENABLE_STAGING YES)
    set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME})
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr) # requirement by AppDir specification
endif()

include(CPack)
