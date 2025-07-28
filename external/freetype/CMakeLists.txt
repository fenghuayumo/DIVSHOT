cmake_minimum_required(VERSION 3.10)
project(freetype C)

add_library(freetype STATIC
    include/freetype/config/ftconfig.h
    include/freetype/config/ftheader.h
    include/freetype/config/ftmodule.h
    include/freetype/config/ftoption.h
    include/freetype/config/ftstdlib.h
    include/ft2build.h
    src/autofit/autofit.c
    src/base/ftbase.c
    src/base/ftbbox.c
    src/base/ftbitmap.c
    src/base/ftdebug.c
    src/base/ftfstype.c
    src/base/ftgasp.c
    src/base/ftglyph.c
    src/base/ftgxval.c
    src/base/ftinit.c
    src/base/ftlcdfil.c
    src/base/ftmm.c
    src/base/ftotval.c
    src/base/ftpatent.c
    src/base/ftpfr.c
    src/base/ftstroke.c
    src/base/ftsynth.c
    src/base/ftsystem.c
    src/base/fttype1.c
    src/base/ftwinfnt.c
    src/bdf/bdf.c
    src/cache/ftcache.c
    src/cff/cff.c
    src/cid/type1cid.c
    src/gzip/ftgzip.c
    src/lzw/ftlzw.c
    src/pcf/pcf.c
    src/pfr/pfr.c
    src/psaux/psaux.c
    src/pshinter/pshinter.c
    src/psnames/psmodule.c
    src/raster/raster.c
    src/sfnt/sfnt.c
    src/smooth/smooth.c
    src/truetype/truetype.c
    src/type1/type1.c
    src/type42/type42.c
    src/winfonts/winfnt.c
)

target_compile_definitions(freetype PRIVATE FT2_BUILD_LIBRARY)

# Disable all warnings
if(MSVC)
    target_compile_options(freetype PRIVATE /w)
else()
    target_compile_options(freetype PRIVATE -w)
endif()

target_include_directories(freetype
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Set runtime library
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_property(TARGET freetype PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
    else()
        set_property(TARGET freetype PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    endif()
endif()

# Set optimization flags based on configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
target_compile_options(freetype PRIVATE -g)
# Release/Production configuration
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(freetype PRIVATE -O2)
elseif(CMAKE_BUILD_TYPE STREQUAL "Production")
    target_compile_options(freetype PRIVATE -O3)
endif()