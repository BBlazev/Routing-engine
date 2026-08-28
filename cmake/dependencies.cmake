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
FetchContent_MakeAvailable(vk-bootstrap)

message(STATUS "Vulkan found:       ${Vulkan_INCLUDE_DIRS}")
message(STATUS "GLFW found:         ${glfw3_DIR}")
message(STATUS "GLM found:          ${glm_DIR}")
message(STATUS "vk-bootstrap:       fetched via FetchContent")
