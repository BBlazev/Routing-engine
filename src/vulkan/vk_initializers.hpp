#ifndef VK_INITIALIZERS_HPP
#define VK_INITIALIZERS_HPP

#include <vulkan/vulkan.h>

namespace vkinit {

inline VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {

	VkCommandPoolCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	info.queueFamilyIndex = queueFamilyIndex;
	return info;
}

inline VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1) {

	VkCommandBufferAllocateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.pNext = nullptr;
	info.commandPool = pool;
	info.commandBufferCount = count;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	return info;
}

inline VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0) {

	VkCommandBufferBeginInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.pNext = nullptr;
	info.pInheritanceInfo = nullptr;
	info.flags = flags;
	return info;
}

inline VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd) {
	
	VkCommandBufferSubmitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = nullptr;
	info.commandBuffer = cmd;
	info.deviceMask = 0;
	return info;
}

inline VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0) {

	VkFenceCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	return info;
}

inline VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0) {

	VkSemaphoreCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	return info;
}

inline VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask,	VkSemaphore semaphore) {

	VkSemaphoreSubmitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	info.pNext = nullptr;
	info.semaphore = semaphore;
	info.stageMask = stageMask;
	info.deviceIndex = 0;
	info.value = 1;
	return info;
}

inline VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd,VkSemaphoreSubmitInfo* signalSemaphoreInfo,	VkSemaphoreSubmitInfo* waitSemaphoreInfo) {

	VkSubmitInfo2 info{};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	info.pNext = nullptr;
	info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0u : 1u;
	info.pWaitSemaphoreInfos = waitSemaphoreInfo;
	info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0u : 1u;
	info.pSignalSemaphoreInfos = signalSemaphoreInfo;
	info.commandBufferInfoCount = 1;
	info.pCommandBufferInfos = cmd;
	return info;
}

inline VkImageCreateInfo image_create_info(VkFormat format,	VkImageUsageFlags usageFlags,VkExtent3D extent) {

	VkImageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent = extent;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usageFlags;
	return info;
}

inline VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image,VkImageAspectFlags aspectFlags) {

	VkImageViewCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.image = image;
	info.format = format;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.aspectMask = aspectFlags;
	return info;
}

inline VkPipelineLayoutCreateInfo pipeline_layout_create_info() {

	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.setLayoutCount = 0;
	info.pSetLayouts = nullptr;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges = nullptr;
	return info;
}

inline VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule,const char* entry = "main") {
	
	VkPipelineShaderStageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.stage = stage;
	info.module = shaderModule;
	info.pName = entry;
	return info;
}

inline VkRenderingAttachmentInfo attachment_info(VkImageView view,const VkClearValue* clear, VkImageLayout layout) {

	VkRenderingAttachmentInfo att{};
	att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	att.pNext = nullptr;
	att.imageView = view;
	att.imageLayout = layout;
	att.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	if (clear) att.clearValue = *clear;
	return att;
}

inline VkRenderingAttachmentInfo depth_attachment_info(VkImageView view, VkImageLayout layout) {

	VkRenderingAttachmentInfo att{};
	att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	att.pNext = nullptr;
	att.imageView = view;
	att.imageLayout = layout;
	att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att.clearValue.depthStencil.depth = 0.0f;
	return att;
}

inline VkRenderingInfo rendering_info(VkExtent2D renderExtent,
	const VkRenderingAttachmentInfo* colorAttachment,
	const VkRenderingAttachmentInfo* depthAttachment) {

	VkRenderingInfo info{};
	info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	info.pNext = nullptr;
	info.renderArea = VkRect2D{ VkOffset2D{0, 0}, renderExtent };
	info.layerCount = 1;
	info.colorAttachmentCount = colorAttachment == nullptr ? 0u : 1u;
	info.pColorAttachments = colorAttachment;
	info.pDepthAttachment = depthAttachment;
	info.pStencilAttachment = nullptr;
	return info;
}

} // namespace vkinit

#endif // VK_INITIALIZERS_HPP