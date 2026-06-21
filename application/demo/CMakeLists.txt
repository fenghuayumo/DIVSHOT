project(schedule_demo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    source/main.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${WKS_LOCATION}/diverse/source
    ${WKS_LOCATION}/diverse/diverse_base/source
)

# External library includes
target_include_directories(${PROJECT_NAME} PUBLIC
    ${ENTT_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
    ${DIVERSE_INCLUDE_DIR}
    ${DIVERSE_BASE_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${OPENAL_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIR}
    ${CEREAL_INCLUDE_DIR}
    ${OZZ_INCLUDE_DIR}
    ${EXTERNAL_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC
    diverse
    diverse_base
    spdlog
    glfw
    imgui
    freetype
    ozz_base
    ozz_animation
    stbimage
    meshoptimizer
    msdf-atlas-gen
    tinyply
)

# Platform-specific definitions and libraries
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        DS_PLATFORM_WINDOWS
        DS_RENDER_API_OPENGL
        DS_RENDER_API_VULKAN
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        DS_IMGUI
        DS_OPENAL
    )

    target_link_libraries(${PROJECT_NAME} PUBLIC
        glfw
        Dbghelp
        OpenGL32
        winmm
    )
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        DS_PLATFORM_MACOS
        DS_PLATFORM_UNIX
        DS_RENDER_API_OPENGL
        DS_RENDER_API_VULKAN
        DS_IMGUI
        DS_OPENAL
    )

    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)

    target_link_libraries(${PROJECT_NAME} PUBLIC
        glfw
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
    )
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        DS_PLATFORM_LINUX
        DS_PLATFORM_UNIX
        DS_RENDER_API_OPENGL
        DS_RENDER_API_VULKAN
        DS_IMGUI
        DS_OPENAL
    )

    target_link_libraries(${PROJECT_NAME} PUBLIC
        glfw
        openal
        X11
        pthread
    )
endif()
