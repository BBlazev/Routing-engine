#ifndef VK_SWAPCHAIN_HPP
#define VK_SWAPCHAIN_HPP

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

class VulkanDevice;

class Swapchain {

public:

	Swapchain() = default;
	~Swapchain();

	Swapchain(const Swapchain&) = delete;
	Swapchain& operator=(const Swapchain&) = delete;

	void init(VulkanDevice& device, uint32_t width, uint32_t height);
	void recreate(uint32_t width, uint32_t height);
	void destroy();

	[[nodiscard]] VkSwapchainKHR handle() const { return swapchain_; }
	[[nodiscard]] VkFormat format() const { return format_; }
	[[nodiscard]] VkExtent2D extent() const { return extent_; }

	[[nodiscard]] uint32_t image_count() const { return static_cast<uint32_t>(images_.size());}
	[[nodiscard]] VkImage image(uint32_t i) const { return images_[i]; }
	[[nodiscard]] VkImageView view(uint32_t i)  const { return views_[i]; }

private:
	void create(uint32_t width, uint32_t height);
	VulkanDevice* device_ = nullptr;
	VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
	VkFormat format_ = VK_FORMAT_UNDEFINED;
	VkExtent2D extent_{};
	std::vector<VkImage> images_;
	std::vector<VkImageView> views_;
};

#endif // VK_SWAPCHAIN_HPP