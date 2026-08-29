include(FetchContent)

# -------------------------------------------------------
# System packages (installed via dnf)
# -------------------------------------------------------
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Threads REQUIRED)

# -------------------------------------------------------
# vk-bootstrap — fetched from GitHub
# Handles Vulkan instance/device creation boilerplate.
# You'll use this in Phase 1 instead of writing 400 lines
# of VkInstanceCreateInfo / VkDeviceCreateInfo.
# -------------------------------------------------------
FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        v1.3.296
    GIT_SHALLOW    ON
)
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG        v3.2.1
    GIT_SHALLOW    ON
)
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
target_compile_options(imgui PRIVATE -w)
FetchContent_MakeAvailable(vma)
FetchContent_MakeAvailable(vk-bootstrap)

message(STATUS "Vulkan found:       ${Vulkan_INCLUDE_DIRS}")
message(STATUS "GLFW found:         ${glfw3_DIR}")
message(STATUS "GLM found:          ${glm_DIR}")
message(STATUS "vk-bootstrap:       fetched via FetchContent")
