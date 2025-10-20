# Project definition
project(diverseshot-cli)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
file(GLOB_RECURSE SOURCES 
    "source/*.hpp"
    "source/*.c"
    "source/*.cpp"
)
if (MSVC)
    set(DIVERSESHOT_CLI_SOURCES ${SOURCES}  diverseshot.rc)
elseif (APPLE)
    set(DIVERSESHOT_CLI_SOURCES ${SOURCES} 
        ${WKS_LOCATION}/application/resource/app_icons/icon128.icns 
        ${WKS_LOCATION}/scripts/mac-run-diverseshot.sh
        ${WKS_LOCATION}/application/resource/app_icons/icon.png
        ${WKS_LOCATION}/application/resource/app_icons/icon32.png
        ${WKS_LOCATION}/application/resource/app_icons/icon128.png
        )
endif()

# target_sources(diverseshot PRIVATE ${DIVERSESHOT_SOURCES})
add_executable(diverseshot-cli ${DIVERSESHOT_CLI_SOURCES})

# Include directories
target_include_directories(diverseshot-cli PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WKS_LOCATION}/diverse/diverse_base/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

# External include directories
target_include_directories(diverseshot-cli PUBLIC
    ${ENTT_INCLUDE_DIR}
    ${CLI_INCLUDE_DIR}
    ${LUA_INCLUDE_DIR}
    ${STB_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${OPENAL_INCLUDE_DIR}
    ${VULKAN_INCLUDE_DIR}
    ${EXTERNAL_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIR}
    ${CEREAL_INCLUDE_DIR}
    ${MSDFGEN_INCLUDE_DIR}
    ${MSDF_ATLAS_GEN_INCLUDE_DIR}
    ${GSTRAIN_INCLUDE_DIR}
    ${GSTRAIN_UTILS_INCLUDE_DIR}
    ${DIVERSE_BASE_INCLUDE_DIR}
    ${OPENCV_INCLUDE_DIR}
    ${TINYGSPLAT_INCLUDE_DIR}
    ${NANOBIND_INCLUDE_DIR}
    ${PYBIND11_INCLUDE_DIR}
    ${OZZ_INCLUDE_DIR}
    ${PY3_INCLUDE_DIR}
)

# target_link_directories(diverseshot-cli PUBLIC
#     ${OPENCV_LIB_DIR}
# )

# Link libraries
target_link_libraries(diverseshot-cli PUBLIC
    diverse_base
    stbimage
    spdlog
    # tinyply
    # tinygsplat
    # gstrain
)

# Defines
target_compile_definitions(diverseshot-cli PUBLIC
    FREEIMAGE_LIB
    "IMGUI_USER_CONFIG=\ <imgui/ImUserConfig.h>\ "
    SPDLOG_COMPILED_LIB
    GLM_FORCE_INTRINSICS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_SWIZZLE
    SPLAT_EDIT
)


# Time trace option
if(TIME_TRACE)
    target_compile_options(diverseshot-cli PRIVATE -ftime-trace)
    target_link_options(diverseshot-cli PRIVATE -ftime-trace)
endif()

# Architecture specific
# if(CMAKE_SIZEOF_VOID_P EQUAL 8)
#     target_compile_definitions(diverseshot-cli PUBLIC USE_VMA_ALLOCATOR)
# endif()

# Platform specific settings
if(WIN32)
    target_compile_definitions(diverseshot-cli PUBLIC
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

    target_link_libraries(diverseshot-cli PUBLIC
        Dbghelp
        # glfw
        # OpenGL32
		# OpenAL32
		# opencv_world490
    )

    target_compile_options(diverseshot-cli PRIVATE /bigobj)
    
elseif(APPLE)
    if(IOS)
        # iOS specific settings
        target_compile_definitions(diverseshot-cli PUBLIC
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
        target_compile_definitions(diverseshot-cli PUBLIC
            DS_PLATFORM_MACOS
            DS_PLATFORM_UNIX
            DS_IMGUI
            DS_OPENAL
            DS_VOLK
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
    
    target_link_libraries(diverseshot-cli PUBLIC
        ${QUARTZCORE_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        # ${OPENAL_FRAMEWORK}
        # ${OpenCV_LIBS}
        # glfw
        # metalirconverter
    )

    target_compile_options(diverseshot-cli PRIVATE
        -Wno-attributes
        -Wno-nullability-completeness
        -fdiagnostics-absolute-paths
    )
    # set_target_properties(diverseshot-cli PROPERTIES
    #                   INSTALL_RPATH "@executable_path/../Frameworks")

elseif(UNIX)
    # Linux specific settings
    target_compile_definitions(diverse-cli PUBLIC
        DS_PLATFORM_LINUX
        DS_PLATFORM_UNIX
        DS_RENDER_API_OPENGL
        DS_RENDER_API_VULKAN
        VK_USE_PLATFORM_XCB_KHR
        DS_IMGUI
        DS_VOLK
        DS_OPENAL
    )
    target_link_libraries(diverseshot-cli PUBLIC
        glfw
        openal
        X11
        pthread
    )

    target_compile_options(diverseshot-cli PRIVATE
        -fpermissive
        -fPIC
        -Wignored-attributes
        -Wno-psabi
    )

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        target_compile_options(diverse-cli PRIVATE -msse4.1)
    endif()

    # set_target_properties(diverse PROPERTIES
    #     LINK_FLAGS "-Wl,-rpath=\\$$ORIGIN"
    # )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug 配置
    add_compile_definitions(
        DS_DEBUG 
        _DEBUG
        TRACY_ENABLE
        DS_PROFILE_ENABLED
        TRACY_ON_DEMAND
    )
    set(CMAKE_DEBUG_POSTFIX "d")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
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
    if(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    endif()
endif()

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    endif()
endif()