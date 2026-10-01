include(FetchContent)
find_package(Threads REQUIRED)


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

if(MSVC)
    target_compile_options(routing_zlib PRIVATE /w)
else()
    target_compile_options(routing_zlib PRIVATE -w)
endif()


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


add_library(routing_osmium INTERFACE)


target_include_directories(routing_osmium SYSTEM INTERFACE
    ${libosmium_SOURCE_DIR}/include
    ${protozero_SOURCE_DIR}/include
)
target_link_libraries(routing_osmium INTERFACE routing_zlib Threads::Threads)

if(MSVC)
    target_compile_definitions(routing_osmium INTERFACE
        NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

add_library(routing STATIC
    src/graph.cpp
    src/parser.cpp
	src/search.cpp
)

target_include_directories(routing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)


target_link_libraries(routing PRIVATE routing_osmium)

if(MSVC)
    target_compile_options(routing PRIVATE /bigobj)
endif()
