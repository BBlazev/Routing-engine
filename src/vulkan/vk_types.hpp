#ifndef VK_TYPES_HPP
#define VK_TYPES_HPP

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>

#include <deque>
#include <functional>
#include <vector>

struct AllocatedBuffer {
	VkBuffer          buffer = VK_NULL_HANDLE;
	VmaAllocation     allocation = nullptr;
	VmaAllocationInfo info{};
};

struct AllocatedImage {
	VkImage       image = VK_NULL_HANDLE;
	VkImageView   imageView = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
	VkExtent3D    imageExtent{};
	VkFormat      imageFormat = VK_FORMAT_UNDEFINED;
};

struct ComputePushConstants {
	glm::vec4 data1{};
	glm::vec4 data2{};
	glm::vec4 data3{};
	glm::vec4 data4{};
};

struct DeletionQueue {
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(std::move(function));
	}

	void flush() {
		for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
			(*it)();
		}
		deletors.clear();
	}
};


struct Vertex {
	glm::vec3 position{};
	float     uv_x = 0.0f;
	glm::vec3 normal{};
	float     uv_y = 0.0f;
	glm::vec4 color{ 1.0f };
};

struct GPUMeshBuffers {
	AllocatedBuffer indexBuffer;
	AllocatedBuffer vertexBuffer;
	VkDeviceAddress vertexBufferAddress = 0;
};

struct GPUDrawPushConstants {
	glm::mat4       worldMatrix{ 1.0f };
	VkDeviceAddress vertexBuffer = 0;
};

struct GPUSceneData {
	glm::mat4 view{ 1.0f };
	glm::mat4 proj{ 1.0f };
	glm::mat4 viewproj{ 1.0f };
	glm::vec4 ambientColor{};
	glm::vec4 sunlightDirection{}; 
	glm::vec4 sunlightColor{};
};

#endif // VK_TYPES_HPP