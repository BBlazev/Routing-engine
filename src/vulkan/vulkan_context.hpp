#ifndef VULKAN_CONTEXT_HPP
#define VULKAN_CONTEXT_HPP

#include <VkBootstrap.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <deque>
#include <functional>

#include <vk_mem_alloc.h>


struct AllocatedImage{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};



struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;
    void push_function(std::function<void()>&& function){
        deletors.push_back(function);
    }

    void flush(){
        for(auto it = deletors.rbegin(); it != deletors.rend(); it++){
            (*it)();
        }
        deletors.clear();
    }
};


struct FrameData {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer mainCommandBuffer = VK_NULL_HANDLE;
    VkSemaphore swapchainSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderSemaphore = VK_NULL_HANDLE;
    VkFence renderFence = VK_NULL_HANDLE;
    DeletionQueue deletionQueue;
};

constexpr unsigned int FRAME_OVERLAP = 4;

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
        void init_sync_structures();
        void draw();


    private:
        vkb::Instance  vkb_instance;
        vkb::Device    vkb_device;
        VkInstance     instance  = VK_NULL_HANDLE;
        VkDevice       device    = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkQueue        graphics_queue = VK_NULL_HANDLE;
        uint32_t       graphics_queue_family = 0;
        VkSurfaceKHR   surface  = VK_NULL_HANDLE;
        DeletionQueue  mainDeletionQueue;
        VmaAllocator allocator;
        AllocatedImage drawImage;
        VkExtent2D drawExtent;


        int frameNumber = 0;

        void create_swapchain(uint32_t width, uint32_t height);
        void destroy_swapchain();

        VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);
        VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);
        VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags =0);
        VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
        VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);
        VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);
        VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
        VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);




};



#endif