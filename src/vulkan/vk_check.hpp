#ifndef VK_CHECK_HPP
#define VK_CHECK_HPP

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <string>

#define VK_CHECK(x)                                                    \
    do {                                                               \
        VkResult result = (x);                                         \
        if (result != VK_SUCCESS) {                                    \
            throw std::runtime_error(                                  \
                std::string{"Vulkan error: "} + std::to_string(result) \
            );                                                         \
        }                                                              \
    } while (0)

#endif