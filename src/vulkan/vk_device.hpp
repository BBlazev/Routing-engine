#ifndef VK_DEVICE_HPP
#define VK_DEVICE_HPP

#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <vulkan/vk_types.hpp>

#include <functional>
#include <span>

struct GLFWwindow;

class VulkanDevice {
public:
	explicit VulkanDevice(GLFWwindow* window);
	~VulkanDevice();

	VulkanDevice(const VulkanDevice&) = delete;
	VulkanDevice& operator=(const VulkanDevice&) = delete;
	VulkanDevice(VulkanDevice&&) = delete;
	VulkanDevice& operator=(VulkanDevice&&) = delete;

	[[nodiscard]] VkInstance instance() const { return instance_; }
	[[nodiscard]] VkPhysicalDevice physical_device() const { return physicalDevice_; }
	[[nodiscard]] VkDevice device() const { return device_; }
	[[nodiscard]] VkSurfaceKHR surface() const { return surface_; }
	[[nodiscard]] VkQueue graphics_queue() const { return graphicsQueue_; }
	[[nodiscard]] uint32_t graphics_queue_family() const { return graphicsQueueFamily_; }
	[[nodiscard]] VmaAllocator allocator() const { return allocator_; }
	[[nodiscard]] const vkb::Device& vkb_device() const { return vkbDevice_; }

	void wait_idle() const;
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);

	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false,
								const char* debugName = nullptr);
	AllocatedImage create_image(const void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false,
								const char* debugName = nullptr);
	void destroy_image(const AllocatedImage& img);

	GPUMeshBuffers upload_mesh(std::span<const uint32_t> indices,std::span<const Vertex> vertices);
	void destroy_mesh(const GPUMeshBuffers& mesh);

	//debugs
	void set_debug_name(uint64_t handle, VkObjectType type, const char* name);
	void begin_label(VkCommandBuffer cmd, const char* name);
	void end_label(VkCommandBuffer cmd);

private:

	void create_instance_and_surface(GLFWwindow* window);
	void select_physical_device();
	void create_logical_device();
	void create_allocator();
	void create_immediate_context();

	vkb::Instance       vkbInstance_{};
	vkb::PhysicalDevice vkbPhysicalDevice_{};
	vkb::Device         vkbDevice_{};
	VkInstance       instance_ = VK_NULL_HANDLE;
	VkSurfaceKHR     surface_ = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
	VkDevice         device_ = VK_NULL_HANDLE;
	VkQueue          graphicsQueue_ = VK_NULL_HANDLE;
	uint32_t         graphicsQueueFamily_ = 0;
	VmaAllocator     allocator_ = nullptr;
	VkFence         immFence_ = VK_NULL_HANDLE;
	VkCommandPool   immCommandPool_ = VK_NULL_HANDLE;
	VkCommandBuffer immCommandBuffer_ = VK_NULL_HANDLE;

	//debugs
	PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT_ = nullptr;
	PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT_ = nullptr;
	PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT_ = nullptr;
	void load_debug_utils();


};

#endif // VK_DEVICE_HPP