#ifndef VULKAN_CONTEXT_HPP
#define VULKAN_CONTEXT_HPP

#include <VkBootstrap.h>
#include <GLFW/glfw3.h>

class VulkanContext{
    public:
        VulkanContext(GLFWwindow* window);
        ~VulkanContext();
        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;
        VulkanContext(VulkanContext&&) = delete;
        VulkanContext& operator=(VulkanContext&&) = delete;


    private:
        vkb::Instance  vkb_instance;
        vkb::Device    vkb_device;
        VkInstance     instance  = VK_NULL_HANDLE;
        VkDevice       device    = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkQueue        graphics_queue = VK_NULL_HANDLE;
        uint32_t       graphics_queue_family = 0;
        VkSurfaceKHR   surface  = VK_NULL_HANDLE;
        
};



#endif