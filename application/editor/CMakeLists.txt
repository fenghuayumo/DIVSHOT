# Project definition
project(diverseshot)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
file(GLOB_RECURSE SOURCES 
    "source/*.h"
    "source/*.c"
    "source/*.cpp"
)
if (MSVC)
    set(DIVERSESHOT_SOURCES ${SOURCES}  diverseshot.rc)
elseif (APPLE)
    set(CMAKE_XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME "YES")
    set(DIVERSESHOT_SOURCES ${SOURCES} 
        ${WKS_LOCATION}/application/resource/app_icons/icon128.icns 
        ${WKS_LOCATION}/scripts/mac-run-diverseshot.sh
        ${WKS_LOCATION}/application/resource/app_icons/icon.png
        ${WKS_LOCATION}/application/resource/app_icons/icon32.png
        ${WKS_LOCATION}/application/resource/app_icons/icon128.png
        )
endif()

# target_sources(diverseshot PRIVATE ${DIVERSESHOT_SOURCES})
add_executable(diverseshot ${DIVERSESHOT_SOURCES})

if (APPLE)
    set(RESOURCE_FILES
        ${WKS_LOCATION}/application/resource/app_icons/icon128.icns
        # ${WKS_LOCATION}/scripts/mac-run-diverseshot.sh
        ${WKS_LOCATION}/application/resource/app_icons/icon.png
        ${WKS_LOCATION}/application/resource/app_icons/icon32.png
        ${WKS_LOCATION}/application/resource/app_icons/icon128.png
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/default.metallib
    )
    set_target_properties(diverseshot PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${WKS_LOCATION}/diverse/source/platform/macos/Info.plist"
        MACOSX_BUNDLE_BUNDLE_VERSION ${DIVERSE_VERSION_ARCH}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${DIVERSE_VERSION}
        MACOSX_BUNDLE_LONG_VERSION_STRING ${DIVERSE_VERSION_ARCH}
        RESOURCE "${RESOURCE_FILES}"
    )
    set_source_files_properties( 
        ${WKS_LOCATION}/application/resource/app_icons/icon.png
        PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    set_source_files_properties( 
        ${WKS_LOCATION}/application/resource/app_icons/icon32.png
        PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    set_source_files_properties( 
        ${WKS_LOCATION}/application/resource/app_icons/icon128.png
        PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    set_source_files_properties(${WKS_LOCATION}/application/resource/app_icons/icon128.icns
        PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    set_source_files_properties(${WKS_LOCATION}/diverse/source/platform/macos/Info.plist
        PROPERTIES
        MACOSX_PACKAGE_LOCATION ""
    )
endif()
# Include directories
target_include_directories(diverseshot PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WKS_LOCATION}/diverse/diverse/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

# External include directories
target_include_directories(diverseshot PUBLIC
    ${ENTT_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
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
    ${DIVERSE_INCLUDE_DIR}
    ${OPENCV_INCLUDE_DIR}
    ${TINYGSPLAT_INCLUDE_DIR}
    ${NANOBIND_INCLUDE_DIR}
    ${PYBIND11_INCLUDE_DIR}
    ${OZZ_INCLUDE_DIR}
    ${PY3_INCLUDE_DIR}
)

# target_link_directories(diverseshot PUBLIC
#     ${OPENCV_LIB_DIR}
# )

# Link libraries
target_link_libraries(diverseshot PUBLIC
    diverse
    stbimage
    freetype
    imgui
    spdlog
    meshoptimizer
    msdf-atlas-gen
    ozz_base
    ozz_animation
    ozz_animation_offline
    tinyply
    tinygsplat
    gstrain
)

# Defines
target_compile_definitions(diverseshot PUBLIC
    FREEIMAGE_LIB
    "IMGUI_USER_CONFIG=\ <imgui/ImUserConfig.h>\ "
    SPDLOG_COMPILED_LIB
    GLM_FORCE_INTRINSICS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_SWIZZLE
    DS_SPLAT_TRAIN
    SPLAT_EDIT
)


# Time trace option
if(TIME_TRACE)
    target_compile_options(diverseshot PRIVATE -ftime-trace)
    target_link_options(diverseshot PRIVATE -ftime-trace)
endif()

# Architecture specific
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_definitions(diverseshot PUBLIC USE_VMA_ALLOCATOR)
endif()

# Platform specific settings
if(WIN32)
    target_compile_definitions(diverseshot PUBLIC
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

    target_link_libraries(diverseshot PUBLIC
        Dbghelp
        glfw
        OpenGL32
		OpenAL32
		opencv_world490
    )

    target_compile_options(diverseshot PRIVATE /bigobj)
    
elseif(APPLE)
    if(IOS)
        # iOS specific settings
        target_compile_definitions(diverseshot PUBLIC
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
        target_compile_definitions(diverseshot PUBLIC
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
    
    target_link_libraries(diverseshot PUBLIC
        ${QUARTZCORE_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        ${OPENAL_FRAMEWORK}
        ${OpenCV_LIBS}
        glfw
        # metalirconverter
    )

    target_compile_options(diverseshot PRIVATE
        -Wno-attributes
        -Wno-nullability-completeness
        -fdiagnostics-absolute-paths
    )
    set_target_properties(diverseshot PROPERTIES
                      INSTALL_RPATH "@executable_path/../Frameworks")


    add_custom_command(
        TARGET diverseshot POST_BUILD
        # create Frameworks directory
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:diverseshot>/../Frameworks
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:diverseshot>/../Resources/fonts
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:diverseshot>/../Resources/vulkan/icd.d
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:diverseshot>/../layouts

        COMMAND ${CMAKE_COMMAND} -E copy  /usr/local/lib/libvulkan.1.dylib $<TARGET_FILE_DIR:diverseshot>/../Frameworks/libvulkan.1.dylib
        COMMAND ${CMAKE_COMMAND} -E copy  /usr/local/lib/libMoltenVK.dylib $<TARGET_FILE_DIR:diverseshot>/../Frameworks/libMoltenVK.dylib
        
        COMMAND ${CMAKE_COMMAND} -E copy  ${CMAKE_SOURCE_DIR}/application/resource/fonts/simkai.ttf $<TARGET_FILE_DIR:diverseshot>/../Resources/fonts/simkai.ttf
        COMMAND ${CMAKE_COMMAND} -E copy  ${CMAKE_SOURCE_DIR}/layouts/dvui.ini $<TARGET_FILE_DIR:diverseshot>/../layouts/dvui.ini
        COMMAND ${CMAKE_COMMAND} -E copy  ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/default.metallib $<TARGET_FILE_DIR:diverseshot>/../Resources/default.metallib
        # excute python3 create_vkicd.py to create vk_icd.json
        COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/create_vkicd.py --resource_path "$<TARGET_FILE_DIR:diverseshot>/../Resources"
        # COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/post-build.sh "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$CONFIGURATION/diverseshot.app"
        COMMENT "copy  vulkan  lib to bin $<TARGET_FILE_DIR:diverseshot>"
    )

elseif(UNIX)
    # Linux specific settings
    target_compile_definitions(diverseshot PUBLIC
        DS_PLATFORM_LINUX
        DS_PLATFORM_UNIX
        DS_RENDER_API_OPENGL
        DS_RENDER_API_VULKAN
        VK_USE_PLATFORM_XCB_KHR
        DS_IMGUI
        DS_VOLK
        DS_OPENAL
    )
    target_link_libraries(diverseshot PUBLIC
        glfw
        openal
        X11
        pthread
    )

    target_compile_options(diverseshot PRIVATE
        -fpermissive
        -fPIC
        -Wignored-attributes
        -Wno-psabi
    )

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        target_compile_options(diverseshot PRIVATE -msse4.1)
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