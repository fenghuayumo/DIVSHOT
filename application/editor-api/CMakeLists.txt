project(editor_api)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EDITOR_API_SOURCES
    source/editor_api.cpp
    source/ipc_server.cpp
)

set(EDITOR_API_HEADERS
    include/editor_api/editor_api.h
    include/editor_api/editor_types.h
    source/ipc_server.h
)

add_library(editor_api SHARED ${EDITOR_API_SOURCES} ${EDITOR_API_HEADERS})

target_include_directories(editor_api
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_compile_definitions(editor_api
    PRIVATE
        EDITOR_API_EXPORT
        EDITOR_API_SHARED
)

if(MSVC)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES
        include/editor_api/editor_api.h
        include/editor_api/editor_types.h
    )
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/source" PREFIX "Source Files" FILES
        source/editor_api.cpp
        source/ipc_server.cpp
        source/ipc_server.h
    )
endif()

set_target_properties(editor_api PROPERTIES
    OUTPUT_NAME "editor_api"
    FOLDER "application"
)

# Sidecar executable for Tauri externalBin
add_executable(divshot-engine source/engine_host_main.cpp)
target_link_libraries(divshot-engine PRIVATE editor_api)
target_include_directories(divshot-engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(divshot-engine PROPERTIES
    OUTPUT_NAME "divshot-engine"
    FOLDER "application"
)

# Copy sidecar for Tauri dev + bundle (externalBin resolves under src-tauri/bin/)
add_custom_command(TARGET divshot-engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../editor-tauri/bin"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../editor-tauri/src-tauri/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:divshot-engine>
        "${CMAKE_CURRENT_SOURCE_DIR}/../editor-tauri/bin/$<TARGET_FILE_NAME:divshot-engine>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:divshot-engine>
        "${CMAKE_CURRENT_SOURCE_DIR}/../editor-tauri/src-tauri/bin/$<TARGET_FILE_NAME:divshot-engine>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:editor_api>
        "${CMAKE_CURRENT_SOURCE_DIR}/../editor-tauri/src-tauri/bin/$<TARGET_FILE_NAME:editor_api>"
    COMMENT "Copy divshot-engine sidecar for editor-tauri"
)
