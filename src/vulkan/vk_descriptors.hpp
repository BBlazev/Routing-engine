#ifndef VK_DESCRIPTORS_HPP
#define VK_DESCRIPTORS_HPP

#include <vulkan/vulkan.h>
#include <vulkan/vk_check.hpp>

#include <deque>
#include <span>
#include <vector>

struct DescriptorLayoutBuilder {

	std::vector<VkDescriptorSetLayoutBinding> bindings;

	void add_binding(uint32_t binding, VkDescriptorType type);
	void clear();
	VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr,
								VkDescriptorSetLayoutCreateFlags flags = 0);
};


class DescriptorAllocatorGrowable {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
	void clear_pools(VkDevice device);
	void destroy_pools(VkDevice device);

	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout,	void* pNext = nullptr);

private:
	VkDescriptorPool get_pool(VkDevice device);
	VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool = 0;
};


struct DescriptorWriter {

	std::deque<VkDescriptorImageInfo>  imageInfos;
	std::deque<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkWriteDescriptorSet>  writes;

	void write_image(int binding, VkImageView image, VkSampler sampler,	VkImageLayout layout, VkDescriptorType type);
	void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,	VkDescriptorType type);
	void clear();
	void update_set(VkDevice device, VkDescriptorSet set);
};

#endif // VK_DESCRIPTORS_HPP