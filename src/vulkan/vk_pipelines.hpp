#ifndef VK_PIPELINES_HPP
#define VK_PIPELINES_HPP

#include <vulkan/vulkan.h>

namespace vkutil {
    bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}

#endif