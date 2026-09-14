#ifndef VK_PIPELINES_HPP
#define VK_PIPELINES_HPP

#include <vulkan/vulkan.h>

#include <filesystem>
#include <vector>


bool load_shader_module(const std::filesystem::path& filePath, VkDevice device,	VkShaderModule* outShaderModule);

class PipelineBuilder {
public:
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	VkPipelineInputAssemblyStateCreateInfo       inputAssembly{};
	VkPipelineRasterizationStateCreateInfo       rasterizer{};
	VkPipelineColorBlendAttachmentState          colorBlendAttachment{};
	VkPipelineMultisampleStateCreateInfo         multisampling{};
	VkPipelineLayout                             pipelineLayout = VK_NULL_HANDLE;
	VkPipelineDepthStencilStateCreateInfo        depthStencil{};
	VkPipelineRenderingCreateInfo                renderInfo{};
	VkFormat                                     colorAttachmentFormat = VK_FORMAT_UNDEFINED;

	PipelineBuilder() { clear(); }

	PipelineBuilder(const PipelineBuilder&) = delete;
	PipelineBuilder& operator=(const PipelineBuilder&) = delete;
	PipelineBuilder(PipelineBuilder&&) = delete;
	PipelineBuilder& operator=(PipelineBuilder&&) = delete;

	void clear();

	void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
	void set_input_topology(VkPrimitiveTopology topology);
	void set_polygon_mode(VkPolygonMode mode);
	void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
	void set_multisampling_none();
	void set_color_attachment_format(VkFormat format);
	void set_depth_format(VkFormat format);

	void disable_blending();
	void enable_blending_additive();
	void enable_blending_alphablend();

	void disable_depthtest();
	void enable_depthtest(bool depthWriteEnable, VkCompareOp op);

	VkPipeline build_pipeline(VkDevice device);
};

#endif // VK_PIPELINES_HPP