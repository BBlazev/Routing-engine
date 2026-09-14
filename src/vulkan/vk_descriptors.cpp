#include <vulkan/vk_descriptors.hpp>

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type) {

	VkDescriptorSetLayoutBinding newbind{};
	newbind.binding = binding;
	newbind.descriptorCount = 1;
	newbind.descriptorType = type;
	bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear() {
	bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext,
													VkDescriptorSetLayoutCreateFlags flags) {

	for (auto& b : bindings) {
		b.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pNext = pNext;
	info.flags = flags;
	info.pBindings = bindings.data();
	info.bindingCount = static_cast<uint32_t>(bindings.size());

	VkDescriptorSetLayout set = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));
	return set;
}


void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) {

	ratios.clear();
	ratios.assign(poolRatios.begin(), poolRatios.end());

	VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios);
	setsPerPool = static_cast<uint32_t>(maxSets * 1.5f);

	readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device) {

	for (auto p : readyPools) {
		vkResetDescriptorPool(device, p, 0);
	}
	for (auto p : fullPools) {
		vkResetDescriptorPool(device, p, 0);
		readyPools.push_back(p);
	}
	fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device) {

	for (auto p : readyPools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	readyPools.clear();

	for (auto p : fullPools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext) {

	VkDescriptorPool poolToUse = get_pool(device);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = pNext;
	allocInfo.descriptorPool = poolToUse;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet ds = VK_NULL_HANDLE;
	VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

	if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
		fullPools.push_back(poolToUse);

		poolToUse = get_pool(device);
		allocInfo.descriptorPool = poolToUse;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
	}
	else {
		VK_CHECK(result);
	}

	readyPools.push_back(poolToUse);
	return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device) {

	VkDescriptorPool newPool = VK_NULL_HANDLE;

	if (!readyPools.empty()) {
		newPool = readyPools.back();
		readyPools.pop_back();
	}
	else {
		newPool = create_pool(device, setsPerPool, ratios);

		setsPerPool = static_cast<uint32_t>(setsPerPool * 1.5f);
		if (setsPerPool > 4092) setsPerPool = 4092;
	}
	return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios) {

	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(poolRatios.size());

	for (const PoolSizeRatio& ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount) });
	}

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = setCount;
	pool_info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	pool_info.pPoolSizes = poolSizes.data();

	VkDescriptorPool newPool = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool));
	return newPool;
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type) {

	VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{ .buffer = buffer, .offset = offset, .range = size });
	VkWriteDescriptorSet write{};

	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = static_cast<uint32_t>(binding);
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;

	writes.push_back(write);
}

void DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type) {

	VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{.sampler = sampler, .imageView = image, .imageLayout = layout });
	VkWriteDescriptorSet write{};

	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = static_cast<uint32_t>(binding);
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.push_back(write);
}

void DescriptorWriter::clear() {
	imageInfos.clear();
	bufferInfos.clear();
	writes.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set) {

	for (VkWriteDescriptorSet& write : writes) {
		write.dstSet = set;
	}
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(),	0, nullptr);
}