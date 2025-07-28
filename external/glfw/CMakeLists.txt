cmake_minimum_required(VERSION 3.10)
project(glfw C)

# Create static library
add_library(glfw STATIC)

# Common source files
target_sources(glfw PRIVATE
    src/context.c
    src/init.c
    src/input.c
    src/monitor.c
    src/vulkan.c
    src/window.c
)

target_include_directories(glfw PUBLIC include)

# Debug configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(glfw PRIVATE -g)
# Release/Production configuration
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(glfw PRIVATE -O2)
elseif(CMAKE_BUILD_TYPE STREQUAL "Production")
    target_compile_options(glfw PRIVATE -O3)
endif()

# Platform-specific configurations
if(WIN32)
    target_compile_definitions(glfw PRIVATE 
        _GLFW_WIN32
        _CRT_SECURE_NO_WARNINGS
    )
    
    target_sources(glfw PRIVATE
        src/win32_platform.h
        src/win32_joystick.h
        src/wgl_context.h
        src/egl_context.h
        src/osmesa_context.h
        src/win32_init.c
        src/win32_joystick.c
        src/win32_monitor.c
        src/win32_time.c
        src/win32_window.c
        src/win32_thread.c
        src/wgl_context.c
        src/egl_context.c
        src/osmesa_context.c
    )

elseif(APPLE)
    target_compile_definitions(glfw PRIVATE 
        _GLFW_COCOA 
        _GLFW_USE_RETINA
    )
    
    target_sources(glfw PRIVATE
        src/cocoa_platform.h
        src/cocoa_joystick.h
        src/nsgl_context.h
        src/cocoa_init.m
        src/cocoa_joystick.m
        src/cocoa_monitor.m
        src/cocoa_window.m
        src/cocoa_time.c
        src/nsgl_context.m
        src/posix_thread.h
        src/posix_thread.c
        src/egl_context.h
        src/egl_context.c
        src/osmesa_context.h
        src/osmesa_context.c
    )
    
    # Enable Objective-C for macOS build
    enable_language(OBJC)

elseif(UNIX AND NOT APPLE)
    target_compile_definitions(glfw PRIVATE 
        _GLFW_X11
        _GLFW_HAS_XF86VM
    )
    
    target_sources(glfw PRIVATE
        src/egl_context.h
        src/egl_context.c
        src/glx_context.h
        src/glx_context.c
        src/linux_joystick.h
        src/linux_joystick.c
        src/posix_time.h
        src/posix_time.c
        src/x11_init.c
        src/x11_platform.h
        src/x11_monitor.c
        src/x11_window.c
        src/xkb_unicode.h
        src/xkb_unicode.c
        src/posix_thread.h
        src/posix_thread.c
        src/osmesa_context.c
        src/osmesa_context.h
    )
    
    target_compile_options(glfw PRIVATE -fPIC)
endif()