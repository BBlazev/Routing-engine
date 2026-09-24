# ---------------------------------------------------------------------------
# routing -- OSM parsing, road graph, shortest paths.
#
# Knows nothing about Vulkan. The engine links it, and so does routing_cli,
# which lets you test routing from a console without opening a window.
#
# This file owns its own dependencies, so the library can be understood (and
# built) on its own.
# ---------------------------------------------------------------------------

include(FetchContent)
find_package(Threads REQUIRED)

# The trick used for all three dependencies below: SOURCE_SUBDIR points at a
# folder that doesn't exist, so FetchContent downloads the source but does
# NOT run the dependency's own CMakeLists.txt. We only want their files, not
# their tests, examples, install rules and find-modules.

# --- zlib -------------------------------------------------------------------
# PBF data blocks are zlib-compressed, so osmium needs it. Built from source
# rather than found on the system: on Windows, find_package(ZLIB) tends to pick
# up a zlib some other tool installed, often built against a different C
# runtime, which fails at link time in confusing ways.
#
# Only the core files -- the gz* file API is skipped because osmium's PBF
# reader never uses it.
enable_language(C)

FetchContent_Declare(zlib
    GIT_REPOSITORY https://github.com/madler/zlib
    GIT_TAG        v1.3.1
    GIT_SHALLOW    ON
    SOURCE_SUBDIR  do-not-add-subdirectory
)
FetchContent_MakeAvailable(zlib)

add_library(routing_zlib STATIC
    ${zlib_SOURCE_DIR}/adler32.c
    ${zlib_SOURCE_DIR}/compress.c
    ${zlib_SOURCE_DIR}/crc32.c
    ${zlib_SOURCE_DIR}/deflate.c
    ${zlib_SOURCE_DIR}/infback.c
    ${zlib_SOURCE_DIR}/inffast.c
    ${zlib_SOURCE_DIR}/inflate.c
    ${zlib_SOURCE_DIR}/inftrees.c
    ${zlib_SOURCE_DIR}/trees.c
    ${zlib_SOURCE_DIR}/uncompr.c
    ${zlib_SOURCE_DIR}/zutil.c
)
target_include_directories(routing_zlib SYSTEM PUBLIC ${zlib_SOURCE_DIR})

# Third-party C code: don't drown our own warnings in theirs.
if(MSVC)
    target_compile_options(routing_zlib PRIVATE /w)
else()
    target_compile_options(routing_zlib PRIVATE -w)
endif()

# --- protozero + libosmium ---------------------------------------------------
# Both header-only. protozero decodes the protobuf format that PBF files use;
# libosmium is the OSM library on top of it.
FetchContent_Declare(protozero
    GIT_REPOSITORY https://github.com/mapbox/protozero
    GIT_TAG        v1.8.2
    GIT_SHALLOW    ON
    SOURCE_SUBDIR  do-not-add-subdirectory
)
FetchContent_Declare(libosmium
    GIT_REPOSITORY https://github.com/osmcode/libosmium
    GIT_TAG        v2.23.1
    GIT_SHALLOW    ON
    SOURCE_SUBDIR  do-not-add-subdirectory
)
FetchContent_MakeAvailable(protozero libosmium)

# An INTERFACE library compiles nothing. It's a named bundle of settings
# (include dirs, defines, link deps) that anything linking it inherits.
add_library(routing_osmium INTERFACE)

# SYSTEM: treat these as third-party headers, so warnings inside them are
# suppressed. Without it, -Wconversion and /W4 produce hundreds of lines from
# osmium alone.
target_include_directories(routing_osmium SYSTEM INTERFACE
    ${libosmium_SOURCE_DIR}/include
    ${protozero_SOURCE_DIR}/include
)
target_link_libraries(routing_osmium INTERFACE routing_zlib Threads::Threads)

if(MSVC)
    # windows.h otherwise defines min/max macros that break std::min/std::max.
    target_compile_definitions(routing_osmium INTERFACE
        NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

# --- the library itself ------------------------------------------------------
add_library(routing STATIC
    src/graph.cpp
    src/parser.cpp
)

# PUBLIC: anyone who links routing needs to find <routing/graph.hpp>.
target_include_directories(routing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# PRIVATE: only parser.cpp touches osmium. Linkers of routing inherit nothing
# from here except the libraries needed to link (CMake handles that part).
target_link_libraries(routing PRIVATE routing_osmium)

if(MSVC)
    # osmium is template-heavy; a debug build of parser.cpp can exceed the
    # default number of sections in one object file.
    target_compile_options(routing PRIVATE /bigobj)
endif()
