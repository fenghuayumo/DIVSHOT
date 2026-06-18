add_executable(shader_pak_builder
    main.cpp
)

target_link_libraries(shader_pak_builder PRIVATE diverse_base)

target_include_directories(shader_pak_builder PRIVATE
    ${CMAKE_SOURCE_DIR}/diverse/diverse_base/source
)
