
# Option to build demo instead of editor
option(BUILD_DEMO "Build schedule demo application instead of editor" OFF)

if(BUILD_DEMO)
    add_subdirectory(demo)
    message(STATUS "Building demo application (editor disabled)")
    set(MAIN_TARGET "schedule_demo" CACHE STRING "" FORCE)
else()
    add_subdirectory(editor)
    # diverseshot-cli removed as it's no longer needed
    message(STATUS "Building editor application")
    set(MAIN_TARGET "divshot" CACHE STRING "" FORCE)
endif()
