#ifndef VULKAN_CONTEXT_HPP
#define VULKAN_CONTEXT_HPP

#include <VkBootstrap.h>
#include <GLFW/glfw3.h>
#include <vector>

struct FrameData {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer mainCommandBuffer = VK_NULL_HANDLE;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanContext{
    public:

        VulkanContext(GLFWwindow* window);
        ~VulkanContext();

        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;
        VulkanContext(VulkanContext&&) = delete;
        VulkanContext& operator=(VulkanContext&&) = delete;

        FrameData frames[FRAME_OVERLAP];
        FrameData& get_current_frame() {return frames[frameNumber % FRAME_OVERLAP];};


        VkSwapchainKHR swapchain;
        VkFormat swapchainImageFormat;

        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        VkExtent2D swapchainExtent;

        [[nodiscard]] VkDevice get_device() const { return device; }
        [[nodiscard]] VkQueue get_graphics_queue() const { return graphics_queue; }
        [[nodiscard]] uint32_t get_graphics_queue_family() const { return graphics_queue_family; }

        void init_vulkan();
        void init_swapchain();
        void init_commands();

    private:
        vkb::Instance  vkb_instance;
        vkb::Device    vkb_device;
        VkInstance     instance  = VK_NULL_HANDLE;
        VkDevice       device    = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkQueue        graphics_queue = VK_NULL_HANDLE;
        uint32_t       graphics_queue_family = 0;
        VkSurfaceKHR   surface  = VK_NULL_HANDLE;
        
        int frameNumber = 0;

        void create_swapchain(uint32_t width, uint32_t height);
        void destroy_swapchain();

};



#endif