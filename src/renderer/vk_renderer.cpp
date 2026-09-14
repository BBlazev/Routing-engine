#include <renderer/vk_renderer.hpp>

#include <core/paths.hpp>
#include <platform/window.hpp>
#include <renderer/vk_pipelines.hpp>
#include <vulkan/vk_check.hpp>
#include <vulkan/vk_images.hpp>
#include <vulkan/vk_initializers.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
constexpr uint64_t kOneSecondInNanoseconds = 1'000'000'000;
}



Renderer::Renderer(Window& window, VulkanDevice& device) : window_(window), device_(device) {

	/*
		if object that we run for threw in its constructor, cleanup and rethrow
	*/
	try {
		init_swapchain();
		init_render_targets();
		init_commands();
		init_sync_structures();
		init_descriptors();
		init_pipelines();
		init_imgui();
		init_default_data();
	}
	catch (...) {
		destroy();
		throw;
	}

	window_.set_resize_callback([this](int, int) { resizeRequested_ = true; });
}

Renderer::~Renderer() {
	destroy();
}

void Renderer::destroy() noexcept {

	if (destroyed_) return;
	destroyed_ = true;

	device_.wait_idle();
	window_.set_resize_callback(nullptr);

	for (auto& mesh : meshes_) {
		device_.destroy_mesh(mesh->meshBuffers);
	}
	meshes_.clear();

	for (auto& frame : frames_) {  

		vkDestroyCommandPool(device_.device(), frame.commandPool, nullptr);
		vkDestroyFence(device_.device(), frame.renderFence, nullptr);
		vkDestroySemaphore(device_.device(), frame.renderSemaphore, nullptr);
		vkDestroySemaphore(device_.device(), frame.swapchainSemaphore, nullptr);

		frame.deletionQueue.flush();
		frame.frameDescriptors.destroy_pools(device_.device());
	}

	mainDeletionQueue_.flush();
	swapchain_.destroy();
}


void Renderer::init_swapchain() {
	const VkExtent2D fb = window_.framebuffer_extent();
	swapchain_.init(device_, fb.width, fb.height);
}

void Renderer::init_render_targets() {

	const VkExtent3D drawImageExtent{ settings::DRAW_IMAGE_WIDTH, settings::DRAW_IMAGE_HEIGHT, 1 };

	VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	drawImage_ = device_.create_image(drawImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT,drawImageUsages);

	depthImage_ = device_.create_image(drawImageExtent, VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	mainDeletionQueue_.push_function([this]() {
		device_.destroy_image(drawImage_);
		device_.destroy_image(depthImage_);
	});
}

void Renderer::init_commands() {
	VkCommandPoolCreateInfo poolInfo = vkinit::command_pool_create_info(
		device_.graphics_queue_family(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (auto& frame : frames_) {

		VK_CHECK(vkCreateCommandPool(device_.device(), &poolInfo, nullptr, &frame.commandPool));
		VkCommandBufferAllocateInfo allocInfo =	vkinit::command_buffer_allocate_info(frame.commandPool, 1);
		VK_CHECK(vkAllocateCommandBuffers(device_.device(), &allocInfo,	&frame.mainCommandBuffer));
	}
}

void Renderer::init_sync_structures() {

	VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();

	for (auto& frame : frames_) {

		VK_CHECK(vkCreateFence(device_.device(), &fenceInfo, nullptr, &frame.renderFence));
		VK_CHECK(vkCreateSemaphore(device_.device(), &semaphoreInfo, nullptr,&frame.swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(device_.device(), &semaphoreInfo, nullptr,&frame.renderSemaphore));
	}
}

void Renderer::init_descriptors() {

	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> globalSizes = {	{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} };
	globalDescriptorAllocator_.init(device_.device(), 10, globalSizes);

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		drawImageDescriptorLayout_ = builder.build(device_.device(), VK_SHADER_STAGE_COMPUTE_BIT);
	}

	drawImageDescriptors_ =	globalDescriptorAllocator_.allocate(device_.device(), drawImageDescriptorLayout_);

	{
		DescriptorWriter writer;
		writer.write_image(0, drawImage_.imageView, VK_NULL_HANDLE,	VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.update_set(device_.device(), drawImageDescriptors_);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		sceneDataDescriptorLayout_ = builder.build(	device_.device(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	mainDeletionQueue_.push_function([this]() {
		globalDescriptorAllocator_.destroy_pools(device_.device());
		vkDestroyDescriptorSetLayout(device_.device(), drawImageDescriptorLayout_, nullptr);
		vkDestroyDescriptorSetLayout(device_.device(), sceneDataDescriptorLayout_, nullptr);
	});

	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
	};

	for (auto& frame : frames_) {
		frame.frameDescriptors.init(device_.device(), 1000, frameSizes);
	}
}

void Renderer::init_pipelines() {

	init_background_pipeline();
	init_mesh_pipeline();

}

void Renderer::init_background_pipeline() {
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(ComputePushConstants);

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &drawImageDescriptorLayout_;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	VK_CHECK(vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr,	&gradientPipelineLayout_));

	VkShaderModule computeShader = VK_NULL_HANDLE;
	if (!load_shader_module(paths::shader("gradient_color.comp.spv"), device_.device(),
		&computeShader)) {
		throw std::runtime_error(
			"Could not load gradient_color.comp.spv. Searched:" +
			paths::describe_search("shaders", "gradient_color.comp.spv"));
	}

	VkComputePipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	createInfo.layout = gradientPipelineLayout_;
	createInfo.stage = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, computeShader);

	VK_CHECK(vkCreateComputePipelines(device_.device(), VK_NULL_HANDLE, 1, &createInfo,	nullptr, &gradientPipeline_));

	vkDestroyShaderModule(device_.device(), computeShader, nullptr);

	mainDeletionQueue_.push_function([this]() {
		vkDestroyPipelineLayout(device_.device(), gradientPipelineLayout_, nullptr);
		vkDestroyPipeline(device_.device(), gradientPipeline_, nullptr);
	});
}

void Renderer::init_mesh_pipeline() {

	VkShaderModule vertexShader = VK_NULL_HANDLE;
	VkShaderModule fragmentShader = VK_NULL_HANDLE;

	if (!load_shader_module(paths::shader("colored_triangle_mesh.vert.spv"), device_.device(), &vertexShader)) {

		throw std::runtime_error(
			"Could not load colored_triangle_mesh.vert.spv. Searched:" +
			paths::describe_search("shaders", "colored_triangle_mesh.vert.spv"));
	}
	if (!load_shader_module(paths::shader("colored_triangle.frag.spv"), device_.device(),
		&fragmentShader)) {
		vkDestroyShaderModule(device_.device(), vertexShader, nullptr);

		throw std::runtime_error(
			"Could not load colored_triangle.frag.spv. Searched:" +
			paths::describe_search("shaders", "colored_triangle.frag.spv"));
	}

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &sceneDataDescriptorLayout_;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &bufferRange;

	VK_CHECK(vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr,	&meshPipelineLayout_));

	PipelineBuilder builder;
	builder.pipelineLayout = meshPipelineLayout_;
	builder.set_shaders(vertexShader, fragmentShader);
	builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	builder.set_multisampling_none();
	builder.disable_blending();
	builder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	builder.set_color_attachment_format(drawImage_.imageFormat);
	builder.set_depth_format(depthImage_.imageFormat);

	meshPipeline_ = builder.build_pipeline(device_.device());

	vkDestroyShaderModule(device_.device(), vertexShader, nullptr);
	vkDestroyShaderModule(device_.device(), fragmentShader, nullptr);

	mainDeletionQueue_.push_function([this]() {
		vkDestroyPipelineLayout(device_.device(), meshPipelineLayout_, nullptr);
		vkDestroyPipeline(device_.device(), meshPipeline_, nullptr);
	});
}

void Renderer::init_imgui() {

	VkDescriptorPoolSize poolSizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
	poolInfo.pPoolSizes = poolSizes;

	VkDescriptorPool imguiPool = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &imguiPool));

	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForVulkan(window_.handle(), true);

	static VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
	swapchainFormat = swapchain_.format();

	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.Instance = device_.instance();
	initInfo.PhysicalDevice = device_.physical_device();
	initInfo.Device = device_.device();
	initInfo.Queue = device_.graphics_queue();
	initInfo.QueueFamily = device_.graphics_queue_family();
	initInfo.DescriptorPool = imguiPool;
	initInfo.MinImageCount = swapchain_.image_count();
	initInfo.ImageCount = swapchain_.image_count();
	initInfo.UseDynamicRendering = true;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	initInfo.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainFormat;

	ImGui_ImplVulkan_Init(&initInfo);
	ImGui_ImplVulkan_CreateFontsTexture();

	mainDeletionQueue_.push_function([this, imguiPool]() {
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		vkDestroyDescriptorPool(device_.device(), imguiPool, nullptr);
	});
}

void Renderer::init_default_data() {
	GltfLoadOptions options;
	options.overrideColorsWithNormals = true;

	if (auto loaded = load_gltf_meshes(device_, paths::asset("basicmesh.glb"), options)) {
		meshes_ = std::move(*loaded);
	}
	else {
		std::fprintf(stderr,
			"No meshes loaded. Expected a glTF at %s\n",
			paths::asset("basicmesh.glb").string().c_str());
	}

	selectedMesh_ = std::min<int>(2, static_cast<int>(meshes_.size()) - 1);
}

void Renderer::update(float deltaSeconds) {
	const ImGuiIO& io = ImGui::GetIO();
	const bool inputForCamera = !io.WantCaptureMouse && !io.WantCaptureKeyboard;

	camera_.update(window_, deltaSeconds, inputForCamera);

	sceneData_.view = camera_.view_matrix();
	sceneData_.ambientColor = glm::vec4(0.1f);
	sceneData_.sunlightColor = glm::vec4(1.0f);
	sceneData_.sunlightDirection = glm::vec4(0.0f, 1.0f, 0.5f, 1.0f);
}

void Renderer::build_ui() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowSize(ImVec2(400, 260), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Engine")) {
		ImGui::Text("Frame: %llu", static_cast<unsigned long long>(frameNumber_));
		ImGui::Text("FPS:   %.1f", static_cast<double>(ImGui::GetIO().Framerate));
		ImGui::Text("Draw:  %ux%u", drawExtent_.width, drawExtent_.height);
		ImGui::Separator();

		ImGui::Text("Camera %.1f, %.1f, %.1f", static_cast<double>(camera_.position.x),
			static_cast<double>(camera_.position.y),
			static_cast<double>(camera_.position.z));
		ImGui::SliderFloat("Move speed", &camera_.moveSpeed, 0.5f, 50.0f);
		ImGui::TextUnformatted("Right mouse to look, WASD + QE to move");
		ImGui::Separator();

		if (!meshes_.empty()) {
			ImGui::SliderInt("Mesh", &selectedMesh_, 0,
				static_cast<int>(meshes_.size()) - 1);
			ImGui::TextUnformatted(meshes_[static_cast<size_t>(selectedMesh_)]->name.c_str());
		}
		ImGui::Separator();

		ImGui::Checkbox("Animate background", &animateBackground_);
		ImGui::SliderFloat("Speed", &backgroundAnimSpeed_, 0.0f, 3.0f);
		ImGui::ColorEdit4("Top", &backgroundColorA_.x);
		ImGui::ColorEdit4("Bottom", &backgroundColorB_.x);
	}
	ImGui::End();

	ImGui::Render();
}


void Renderer::handle_resize() {

	while (window_.is_minimised() && !window_.should_close()) {
		window_.wait_events();
	}
	if (window_.should_close()) return;

	const VkExtent2D fb = window_.framebuffer_extent();
	swapchain_.recreate(fb.width, fb.height);
	resizeRequested_ = false;
}

void Renderer::draw() {

	if (resizeRequested_) handle_resize();
	if (window_.is_minimised()) return;

	FrameData& frame = current_frame();

	//wait on render fence, CPU blocks untill GPU frees slot
	VK_CHECK(vkWaitForFences(device_.device(), 1, &frame.renderFence, VK_TRUE, kOneSecondInNanoseconds));

	//recycle frame resources
	frame.deletionQueue.flush();
	frame.frameDescriptors.clear_pools(device_.device());

	build_ui();

	//acquire swapchain image
	uint32_t swapchainImageIndex = 0;
	VkResult acquireResult = vkAcquireNextImageKHR(	device_.device(), swapchain_.handle(), kOneSecondInNanoseconds,
													frame.swapchainSemaphore, VK_NULL_HANDLE, &swapchainImageIndex);

	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resizeRequested_ = true;
		return;  
	}
	if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
		VK_CHECK(acquireResult);
	}

	VK_CHECK(vkResetFences(device_.device(), 1, &frame.renderFence));

	//record command buffer
	VkCommandBuffer cmd = frame.mainCommandBuffer;
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

	drawExtent_.width = std::min(swapchain_.extent().width, drawImage_.imageExtent.width);
	drawExtent_.height = std::min(swapchain_.extent().height, drawImage_.imageExtent.height);

	vkutil::transition_image(cmd, drawImage_.image, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL);
	draw_background(cmd);

	vkutil::transition_image(cmd, drawImage_.image, VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, depthImage_.image, VK_IMAGE_LAYOUT_UNDEFINED,	VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	draw_geometry(cmd);

	vkutil::transition_image(cmd, drawImage_.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, swapchain_.image(swapchainImageIndex), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	vkutil::copy_image_to_image(cmd, drawImage_.image,swapchain_.image(swapchainImageIndex), drawExtent_, swapchain_.extent());

	vkutil::transition_image(cmd, swapchain_.image(swapchainImageIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	draw_imgui(cmd, swapchain_.view(swapchainImageIndex));

	vkutil::transition_image(cmd, swapchain_.image(swapchainImageIndex), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(cmd));

	//submit to graphics queue
	VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
	VkSemaphoreSubmitInfo     waitInfo = vkinit::semaphore_submit_info(	VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, frame.swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.renderSemaphore);

	VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);
	VK_CHECK(vkQueueSubmit2(device_.graphics_queue(), 1, &submit, frame.renderFence));

	VkSwapchainKHR swapchainHandle = swapchain_.handle();

	//present it
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchainHandle;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.renderSemaphore;
	presentInfo.pImageIndices = &swapchainImageIndex;

	VkResult presentResult = vkQueuePresentKHR(device_.graphics_queue(), &presentInfo);

	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
		resizeRequested_ = true;
	}
	else if (presentResult != VK_SUCCESS) {
		VK_CHECK(presentResult);
	}

	frameNumber_++;
}

void Renderer::draw_background(VkCommandBuffer cmd) {

	ComputePushConstants pc{};

	if (animateBackground_) {
		const float t = static_cast<float>(glfwGetTime()) * backgroundAnimSpeed_;
		const float s = 0.5f * (std::sin(t) + 1.0f);
		pc.data1 = glm::mix(backgroundColorA_, backgroundColorB_, s);
		pc.data2 = glm::mix(backgroundColorB_, backgroundColorA_, s);
	}
	else {
		pc.data1 = backgroundColorA_;
		pc.data2 = backgroundColorB_;
	}

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipeline_);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout_, 0,1, &drawImageDescriptors_, 0, nullptr);
	vkCmdPushConstants(cmd, gradientPipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pc);

	vkCmdDispatch(cmd, (drawImage_.imageExtent.width + 15) / 16, (drawImage_.imageExtent.height + 15) / 16, 1);
}

void Renderer::draw_geometry(VkCommandBuffer cmd) {

	if (meshes_.empty() || selectedMesh_ < 0) return;

	const auto& mesh = meshes_[static_cast<size_t>(selectedMesh_)];
	if (mesh->surfaces.empty()) return;

	const float aspect = static_cast<float>(drawExtent_.width) / static_cast<float>(std::max(1u, drawExtent_.height));

	sceneData_.proj = camera_.projection_matrix(aspect, settings::CAMERA_FOV_DEGREES, settings::CAMERA_NEAR, settings::CAMERA_FAR);
	sceneData_.viewproj = sceneData_.proj * sceneData_.view;

	AllocatedBuffer sceneBuffer = device_.create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	*static_cast<GPUSceneData*>(sceneBuffer.info.pMappedData) = sceneData_;

	current_frame().deletionQueue.push_function([this, sceneBuffer]() { device_.destroy_buffer(sceneBuffer); });

	VkDescriptorSet sceneDescriptor = current_frame().frameDescriptors.allocate( device_.device(), sceneDataDescriptorLayout_);

	{
		DescriptorWriter writer;
		writer.write_buffer(0, sceneBuffer.buffer, sizeof(GPUSceneData), 0,	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		writer.update_set(device_.device(), sceneDescriptor);
	}

	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(drawImage_.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(depthImage_.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderInfo = vkinit::rendering_info(drawExtent_, &colorAttachment, &depthAttachment);

	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline_);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipelineLayout_, 0, 1, &sceneDescriptor, 0, nullptr);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(drawExtent_.width);
	viewport.height = static_cast<float>(drawExtent_.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = drawExtent_;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	const glm::mat4 model = glm::rotate(glm::mat4{ 1.0f }, glm::radians(180.0f), glm::vec3{ 0.0f, 1.0f, 0.0f });

	GPUDrawPushConstants pushConstants{};
	pushConstants.worldMatrix = sceneData_.viewproj * model;
	pushConstants.vertexBuffer = mesh->meshBuffers.vertexBufferAddress;

	vkCmdPushConstants(cmd, meshPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,	sizeof(GPUDrawPushConstants), &pushConstants);
	vkCmdBindIndexBuffer(cmd, mesh->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	for (const GeoSurface& surface : mesh->surfaces) {
		vkCmdDrawIndexed(cmd, surface.count, 1, surface.startIndex, 0, 0);
	}

	vkCmdEndRendering(cmd);
}

void Renderer::draw_imgui(VkCommandBuffer cmd, VkImageView targetView) {

	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderInfo = vkinit::rendering_info(swapchain_.extent(), &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	vkCmdEndRendering(cmd);
}