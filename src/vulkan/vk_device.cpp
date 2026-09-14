#include <vulkan/vk_device.hpp>

#include <vulkan/vk_check.hpp>
#include <vulkan/vk_images.hpp>
#include <vulkan/vk_initializers.hpp>

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

VulkanDevice::VulkanDevice(GLFWwindow* window) {
	create_instance_and_surface(window);
	select_physical_device();
	create_logical_device();
	create_allocator();
	create_immediate_context();
}

VulkanDevice::~VulkanDevice() {
	if (device_ != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(device_);

		vkDestroyFence(device_, immFence_, nullptr);
		vkDestroyCommandPool(device_, immCommandPool_, nullptr);
	}

	if (allocator_ != nullptr) {
		vmaDestroyAllocator(allocator_);
	}

	vkb::destroy_device(vkbDevice_);

	if (surface_ != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance_, surface_, nullptr);
	}

	vkb::destroy_instance(vkbInstance_);
}

void VulkanDevice::create_instance_and_surface(GLFWwindow* window) {
	
	auto instanceResult = vkb::InstanceBuilder{}.set_app_name("vulkan-engine").require_api_version(1, 3, 0)
#ifndef NDEBUG
		.request_validation_layers(true)
		.use_default_debug_messenger()
#endif
		.build();

	if (!instanceResult) {
		throw std::runtime_error(std::string{ "Failed to create Vulkan instance: " } +
			instanceResult.error().message());
	}

	vkbInstance_ = instanceResult.value();
	instance_ = vkbInstance_.instance;

	VK_CHECK(glfwCreateWindowSurface(instance_, window, nullptr, &surface_));
}

void VulkanDevice::select_physical_device() {

	auto physResult = vkb::PhysicalDeviceSelector{ vkbInstance_ }
		.set_minimum_version(1, 3)
		.set_surface(surface_)
		.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
		.select();

	if (!physResult) {
		throw std::runtime_error(std::string{ "Failed to select GPU: " } +
			physResult.error().message());
	}

	vkbPhysicalDevice_ = physResult.value();
	physicalDevice_ = vkbPhysicalDevice_.physical_device;

	VkPhysicalDeviceProperties props{};

	vkGetPhysicalDeviceProperties(physicalDevice_, &props);
	std::cout << "GPU:    " << props.deviceName << "\n"
		<< "Vulkan: " << VK_VERSION_MAJOR(props.apiVersion) << "."
		<< VK_VERSION_MINOR(props.apiVersion) << "."
		<< VK_VERSION_PATCH(props.apiVersion) << "\n";
}

void VulkanDevice::create_logical_device() {

	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.bufferDeviceAddress = VK_TRUE;
	features12.descriptorIndexing = VK_TRUE;

	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.synchronization2 = VK_TRUE;
	features13.dynamicRendering = VK_TRUE;

	auto deviceResult = vkb::DeviceBuilder{ vkbPhysicalDevice_ }
		.add_pNext(&features12)
		.add_pNext(&features13)
		.build();

	if (!deviceResult) {
		throw std::runtime_error(std::string{ "Failed to create device: " } + deviceResult.error().message());
	}

	vkbDevice_ = deviceResult.value();
	device_ = vkbDevice_.device;

	auto queueResult = vkbDevice_.get_queue(vkb::QueueType::graphics);
	if (!queueResult) {
		throw std::runtime_error("Failed to get graphics queue");
	}

	graphicsQueue_ = queueResult.value();
	graphicsQueueFamily_ = vkbDevice_.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanDevice::create_allocator() {

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = physicalDevice_;
	allocatorInfo.device = device_;
	allocatorInfo.instance = instance_;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator_));
}

void VulkanDevice::create_immediate_context() {

	VkCommandPoolCreateInfo poolInfo = vkinit::command_pool_create_info(graphicsQueueFamily_, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &immCommandPool_));

	VkCommandBufferAllocateInfo allocInfo = vkinit::command_buffer_allocate_info(immCommandPool_, 1);
	VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &immCommandBuffer_));

	VkFenceCreateInfo fenceInfo = vkinit::fence_create_info();
	VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &immFence_));
}

void VulkanDevice::wait_idle() const {
	if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);
}

void VulkanDevice::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) {

	VK_CHECK(vkResetFences(device_, 1, &immFence_));
	VK_CHECK(vkResetCommandBuffer(immCommandBuffer_, 0));

	VkCommandBuffer cmd = immCommandBuffer_;

	VkCommandBufferBeginInfo beginInfo =
		vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

	VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &submit, immFence_));
	VK_CHECK(vkWaitForFences(device_, 1, &immFence_, VK_TRUE, 9999999999));
}

AllocatedBuffer VulkanDevice::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
	
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = memoryUsage;
	vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	AllocatedBuffer newBuffer{};
	VK_CHECK(vmaCreateBuffer(allocator_, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer,	&newBuffer.allocation, &newBuffer.info));
	return newBuffer;
}

void VulkanDevice::destroy_buffer(const AllocatedBuffer& buffer) {
	if (buffer.buffer == VK_NULL_HANDLE) return;
	vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
}

AllocatedImage VulkanDevice::create_image(VkExtent3D size, VkFormat format,	VkImageUsageFlags usage, bool mipmapped) {
	
	AllocatedImage newImage{};
	newImage.imageFormat = format;
	newImage.imageExtent = size;

	VkImageCreateInfo imgInfo = vkinit::image_create_info(format, usage, size);
	if (mipmapped) {
		imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(allocator_, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));

	VkImageAspectFlags aspectFlag = (format == VK_FORMAT_D32_SFLOAT)
									? VK_IMAGE_ASPECT_DEPTH_BIT
									: VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
	viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

	VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &newImage.imageView));

	return newImage;
}

AllocatedImage VulkanDevice::create_image(const void* data, VkExtent3D size,
									VkFormat format, VkImageUsageFlags usage, bool mipmapped) {

	const size_t dataSize = static_cast<size_t>(size.depth) * size.width * size.height * 4;

	AllocatedBuffer uploadBuffer = create_buffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	std::memcpy(uploadBuffer.info.pMappedData, data, dataSize);

	AllocatedImage newImage = create_image(
								size, format,
								usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
								mipmapped);

	immediate_submit([&](VkCommandBuffer cmd) {
		
		vkutil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = size;

		vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		vkutil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});

	destroy_buffer(uploadBuffer);
	return newImage;
}

void VulkanDevice::destroy_image(const AllocatedImage& img) {
	if (img.imageView != VK_NULL_HANDLE) vkDestroyImageView(device_, img.imageView, nullptr);
	if (img.image != VK_NULL_HANDLE) vmaDestroyImage(allocator_, img.image, img.allocation);
}

GPUMeshBuffers VulkanDevice::upload_mesh(std::span<const uint32_t> indices,
	std::span<const Vertex> vertices) {
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers mesh{};

	mesh.vertexBuffer = create_buffer(vertexBufferSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = mesh.vertexBuffer.buffer;
	mesh.vertexBufferAddress = vkGetBufferDeviceAddress(device_, &addressInfo);

	mesh.indexBuffer = create_buffer(
		indexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
											VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = staging.info.pMappedData;
	std::memcpy(data, vertices.data(), vertexBufferSize);
	std::memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{};
		vertexCopy.srcOffset = 0;
		vertexCopy.dstOffset = 0;
		vertexCopy.size = vertexBufferSize;
		vkCmdCopyBuffer(cmd, staging.buffer, mesh.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{};
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.dstOffset = 0;
		indexCopy.size = indexBufferSize;
		vkCmdCopyBuffer(cmd, staging.buffer, mesh.indexBuffer.buffer, 1, &indexCopy);
	});

	destroy_buffer(staging);
	return mesh;
}

void VulkanDevice::destroy_mesh(const GPUMeshBuffers& mesh) {
	destroy_buffer(mesh.indexBuffer);
	destroy_buffer(mesh.vertexBuffer);
}