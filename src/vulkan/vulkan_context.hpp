#ifndef VULKAN_CONTEXT_HPP
#define VULKAN_CONTEXT_HPP

#include <VkBootstrap.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <deque>
#include <functional>

#include <vk_mem_alloc.h>
#include <vulkan/vk_descriptors.hpp>
#include <vulkan/vk_types.hpp>
#include <rendering/vk_loader.hpp>
#include <rendering/vk_pipelines.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <glm/glm.hpp>
#include <span>



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

        VkExtent2D windowExtent{1600, 1200};

        VkSwapchainKHR swapchain;
        VkFormat swapchainImageFormat;

        DescriptorAllocator globalDescriptorAllocator;
        VkDescriptorSet drawImageDescriptors;
        VkDescriptorSetLayout drawImageDescriptorLayout;
        VkPipeline gradientPipeline;
        VkPipelineLayout gradientPipelineLayout;

        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        VkExtent2D swapchainExtent;

        VkFence immFence = VK_NULL_HANDLE;
        VkCommandBuffer immCommandBuffer = VK_NULL_HANDLE;
        VkCommandPool immCommandPool = VK_NULL_HANDLE;


        ComputePushConstants pushConstants;
        
        
        [[nodiscard]] VkDevice get_device() const { return device; }
        [[nodiscard]] VkQueue get_graphics_queue() const { return graphics_queue; }
        [[nodiscard]] uint32_t get_graphics_queue_family() const { return graphics_queue_family; }
        
        void draw();
        void draw_background(VkCommandBuffer cmd);
        void draw_geometry(VkCommandBuffer cmd);
        void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
    
        GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);


        std::vector<std::shared_ptr<MeshAsset>> testMeshes;

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
        AllocatedImage depthImage;
        VkExtent2D drawExtent;
        GLFWwindow* glfwWindow = nullptr;
        
        VkPipelineLayout trianglePipelineLayout;
        VkPipeline trianglePipeline;
        
        VkPipelineLayout meshPipelineLayout;
        VkPipeline meshPipeline;
        GPUMeshBuffers rectangle;

        //animation
        glm::vec4 colorA{1.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 colorB{0.0f, 0.0f, 1.0f, 1.0f};
        bool animate = true;
        float animSpeed = 0.5f;


        void init_vulkan();
        void init_swapchain();
        void init_commands();
        void init_sync_structures();
        void init_descriptors();
        void init_pipelines();
        void init_background_pipelines();
        void init_imgui();
        void init_mesh_pipeline();
        void init_default_data();

        int frameNumber = 0;

        void create_swapchain(uint32_t width, uint32_t height);
        void destroy_swapchain();
        void destroy_buffer(const AllocatedBuffer& buffer);
        bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);

        VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);
        VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);
        VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags =0);
        VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
        VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);
        VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);
        VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
        VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
        AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
        VkRenderingAttachmentInfo depth_attachment_info(VkImageView view, VkImageLayout layout);
};



#endif