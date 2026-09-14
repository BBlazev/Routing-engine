#include <vulkan/vk_swapchain.hpp>

#include <vulkan/vk_check.hpp>
#include <vulkan/vk_device.hpp>

#include <VkBootstrap.h>

#include <stdexcept>

Swapchain::~Swapchain() {
	destroy();
}

void Swapchain::init(VulkanDevice& device, uint32_t width, uint32_t height) {
	device_ = &device;
	create(width, height);
}

void Swapchain::recreate(uint32_t width, uint32_t height) {
	if (device_ == nullptr) throw std::runtime_error("Swapchain::recreate before init");

	device_->wait_idle();
	destroy();
	create(width, height);
}

void Swapchain::create(uint32_t width, uint32_t height) {

	format_ = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::SwapchainBuilder builder{ device_->vkb_device(), device_->surface() };

	auto result = builder
		.set_desired_format(VkSurfaceFormatKHR{
			.format = format_,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build();

	if (!result) {
		throw std::runtime_error(std::string{ "Failed to create swapchain: " } +
			result.error().message());
	}

	auto vkbSwapchain = result.value();
	swapchain_ = vkbSwapchain.swapchain;
	extent_ = vkbSwapchain.extent;
	images_ = vkbSwapchain.get_images().value();
	views_ = vkbSwapchain.get_image_views().value();
}

void Swapchain::destroy() {

	if (device_ == nullptr) return;

	for (VkImageView view : views_) {
		vkDestroyImageView(device_->device(), view, nullptr);
	}
	views_.clear();
	images_.clear();

	if (swapchain_ != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device_->device(), swapchain_, nullptr);
		swapchain_ = VK_NULL_HANDLE;
	}
}