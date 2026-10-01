#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <core/settings.hpp>
#include <renderer/camera.hpp>
#include <renderer/vk_loader.hpp>
#include <vulkan/vk_descriptors.hpp>
#include <vulkan/vk_device.hpp>
#include <vulkan/vk_swapchain.hpp>
#include <vulkan/vk_types.hpp>

#include <glm/glm.hpp>

#include <memory>
#include <vector>
#include <array>
#include <optional>

class Window;


struct FrameData {

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer mainCommandBuffer = VK_NULL_HANDLE;
	VkSemaphore swapchainSemaphore = VK_NULL_HANDLE;  
	VkSemaphore renderSemaphore = VK_NULL_HANDLE;  
	VkFence renderFence = VK_NULL_HANDLE;  

	DeletionQueue deletionQueue;
	DescriptorAllocatorGrowable frameDescriptors;
};


class Renderer {
public:

	Renderer(Window& window, VulkanDevice& device);
	~Renderer();

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;

	void update(float deltaSeconds);
	void draw();

	struct LineLayer {
		GPUMeshBuffers mesh{};
		uint32_t       indexCount = 0;
	};

	static constexpr uint32_t kLineLayers = 4;
	std::array<LineLayer, kLineLayers> lineLayers_{};

	void set_lines(uint32_t layer, std::span<const uint32_t> indices, std::span<const Vertex> vertices);
	std::optional<glm::vec3> screen_to_ground(double mouseX, double mouseY) const;
	[[nodiscard]] Camera& camera() { return camera_; }

private:

	void init_swapchain();
	void init_render_targets();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_pipelines();
	void init_line_pipeline();
	void init_background_pipeline();
	void init_mesh_pipeline();
	void init_imgui();
	void init_default_data();
	void init_default_samplers();
	void init_default_textures();

	FrameData& current_frame() { return frames_[frameNumber_ % settings::FRAME_OVERLAP]; }

	void build_ui();
	void draw_background(VkCommandBuffer cmd);
	void draw_geometry(VkCommandBuffer cmd);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetView);



	void handle_resize();
	void destroy() noexcept;

	Window& window_;
	VulkanDevice& device_;

	Swapchain swapchain_;
	FrameData frames_[settings::FRAME_OVERLAP];
	uint64_t  frameNumber_ = 0;


	AllocatedImage drawImage_{};
	AllocatedImage depthImage_{};
	AllocatedImage whiteImage_{};
	AllocatedImage greyImage_{};
	AllocatedImage blackImage_{};
	AllocatedImage errorCheckerboardImage_{};

	VkExtent2D     drawExtent_{};

	DescriptorAllocatorGrowable globalDescriptorAllocator_;
	VkDescriptorSet drawImageDescriptors_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout drawImageDescriptorLayout_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout sceneDataDescriptorLayout_ = VK_NULL_HANDLE;

	VkPipeline gradientPipeline_ = VK_NULL_HANDLE;
	VkPipelineLayout gradientPipelineLayout_ = VK_NULL_HANDLE;

	VkPipeline meshPipeline_ = VK_NULL_HANDLE;
	VkPipelineLayout meshPipelineLayout_ = VK_NULL_HANDLE;

	VkPipeline linePipeline_ = VK_NULL_HANDLE;

	DeletionQueue mainDeletionQueue_;

	Camera camera_;
	GPUSceneData sceneData_{};



	VkSampler defaultSamplerLinear_ = VK_NULL_HANDLE;
	VkSampler defaultSamplerNearest_ = VK_NULL_HANDLE;

	std::vector<std::shared_ptr<MeshAsset>> meshes_;
	int selectedMesh_ = 2;

	glm::vec4 backgroundColorA_{ 0.05f, 0.07f, 0.12f, 1.0f };
	glm::vec4 backgroundColorB_{ 0.35f, 0.42f, 0.55f, 1.0f };
	bool animateBackground_ = false;
	float backgroundAnimSpeed_ = 0.5f;

	bool resizeRequested_ = false;
	bool destroyed_ = false;
};

#endif // RENDERER_HPP