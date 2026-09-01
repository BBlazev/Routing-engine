include(FetchContent)

# -------------------------------------------------------
# Vulkan — must be installed (SDK on Windows, dnf on Linux)
# -------------------------------------------------------
find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

# -------------------------------------------------------
# GLFW — system if available, otherwise fetch
# -------------------------------------------------------
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG        3.4
        GIT_SHALLOW    ON
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# -------------------------------------------------------
# GLM — system if available, otherwise fetch
# -------------------------------------------------------
find_package(glm QUIET)
if(NOT glm_FOUND)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG        1.0.1
        GIT_SHALLOW    ON
    )
    FetchContent_MakeAvailable(glm)
endif()

# -------------------------------------------------------
# vk-bootstrap
# -------------------------------------------------------
FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        v1.3.296
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(vk-bootstrap)

# -------------------------------------------------------
# VMA
# -------------------------------------------------------
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG        v3.2.1
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(vma)

# -------------------------------------------------------
# ImGui
# -------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG        v1.91.8
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw)

# -------------------------------------------------------
# Suppress warnings from dependencies
# -------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imgui PRIVATE -w)
endif()

message(STATUS "Vulkan found:       ${Vulkan_INCLUDE_DIRS}")
message(STATUS "vk-bootstrap:       fetched via FetchContent")