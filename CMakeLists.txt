cmake_minimum_required(VERSION 3.25)

project(vulkan-streaming
    VERSION   0.1.0
    LANGUAGES CXX
)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)            
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)     



# Warning flags are wrapped in $<$<COMPILE_LANGUAGE:CXX>:...> so they only
# reach C++ files. The routing library builds zlib, which is C, and flags like
# -Wold-style-cast are meaningless (and warned about) there.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(PROJECT_CXX_WARNINGS
        -Wall -Wextra -Wpedantic
        -Wconversion -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Wcast-align -Wunused
        -Woverloaded-virtual -Wmisleading-indentation
        -Wnull-dereference -Wdouble-promotion
        -Wformat=2 -Wimplicit-fallthrough
    )
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${PROJECT_CXX_WARNINGS}>")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive->")
endif()


if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()


include(cmake/dependencies.cmake)

# Routing library (OSM parsing, graph, shortest paths). Independent of Vulkan.
add_subdirectory(routing)


add_executable(${PROJECT_NAME}
    src/main.cpp
    src/platform/window.cpp
    src/vulkan/vk_device.cpp
    src/vulkan/vma_impl.cpp
    src/vulkan/vk_descriptors.cpp
	src/vulkan/vk_swapchain.cpp
    src/renderer/vk_pipelines.cpp
    src/renderer/vk_loader.cpp
	src/renderer/camera.cpp
	src/renderer/vk_renderer.cpp
)
    

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    Threads::Threads
    imgui
    stb_image
    fastgltf
    routing
)

target_compile_definitions(vulkan-streaming PRIVATE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
    GLM_ENABLE_EXPERIMENTAL
)

find_program(GLSLC glslc
    HINTS
        $ENV{VULKAN_SDK}/Bin
        ${VULKAN_SDK}/Bin
    REQUIRED
)
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


# Console tool for testing routing without the renderer. It borrows only
# core/paths.hpp from the engine, which is plain std:: code.
add_executable(routing_cli tools/routing_cli/main.cpp)
target_include_directories(routing_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(routing_cli PRIVATE routing)


option(BUILD_TESTS "Build stress tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()


