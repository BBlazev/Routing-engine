cmake_minimum_required(VERSION 3.25)

project(vulkan-streaming
    VERSION   0.1.0
    LANGUAGES CXX
)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)            
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)     


add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wconversion -Wshadow -Wnon-virtual-dtor
    -Wold-style-cast -Wcast-align -Wunused
    -Woverloaded-virtual -Wmisleading-indentation
    -Wnull-dereference -Wdouble-promotion
    -Wformat=2 -Wimplicit-fallthrough
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()


if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()


include(cmake/dependencies.cmake)


add_executable(${PROJECT_NAME}
    src/main.cpp
    src/vulkan/window.cpp
    src/vulkan/vulkan_context.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    vk-bootstrap::vk-bootstrap
    Threads::Threads
)


find_program(GLSLC glslc REQUIRED)

file(GLOB_RECURSE SHADER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp"
)

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND ${GLSLC} ${SHADER} -o ${SPIRV_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader: ${SHADER_NAME}"
    )

    list(APPEND SPIRV_OUTPUTS ${SPIRV_OUTPUT})
endforeach()

add_custom_target(shaders DEPENDS ${SPIRV_OUTPUTS})
add_dependencies(${PROJECT_NAME} shaders)


option(BUILD_TESTS "Build stress tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()


