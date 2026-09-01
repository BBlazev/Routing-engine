#include <vulkan/vulkan_context.hpp>
#include <vulkan/vk_check.hpp>
#include <vulkan/vk_image.hpp>
#include <vulkan/vk_types.hpp>
#include <settings.hpp>
#include <iostream>
#include <cmath>
#include <fstream>
#include <cstdio>
#include <utility>
#include <array>

VkPipelineLayoutCreateInfo pipeline_layout_create_info() {

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

VkRenderingAttachmentInfo attachment_info(VkImageView view, VkClearValue* clear,
                                          VkImageLayout layout) {
    VkRenderingAttachmentInfo att{};
    att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    att.pNext = nullptr;
    att.imageView = view;
    att.imageLayout = layout;
    att.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (clear)
        att.clearValue = *clear;
    return att;
}

VkRenderingInfo rendering_info(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment,
                               VkRenderingAttachmentInfo* depthAttachment) {
    VkRenderingInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.pNext = nullptr;
    info.renderArea = VkRect2D{VkOffset2D{0, 0}, renderExtent};
    info.layerCount = 1;
    info.colorAttachmentCount = 1;
    info.pColorAttachments = colorAttachment;
    info.pDepthAttachment = depthAttachment;
    info.pStencilAttachment = nullptr;
    return info;
}


void VulkanContext::init_vulkan() {
    init_swapchain();
    init_commands();
    init_sync_structures();
    init_descriptors();
    init_default_data();
    init_pipelines();
    init_imgui();
}


VulkanContext::VulkanContext(GLFWwindow* window){

    glfwWindow = window;
    auto instance_result = vkb::InstanceBuilder{}
        .set_app_name("vulkan-project")
        .require_api_version(1, 3, 0)       
        .request_validation_layers(true)    
        .use_default_debug_messenger()      
        .build();

    if (!instance_result) {
        throw std::runtime_error(
            std::string{"Failed to create Vulkan instance: "} +
            instance_result.error().message()
        );
    }

    vkb_instance = instance_result.value();
    instance = vkb_instance.instance;


    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }


    auto phys_result = vkb::PhysicalDeviceSelector{vkb_instance}
        .set_minimum_version(1, 3)
        .set_surface(surface)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .select();

    if (!phys_result) {
        throw std::runtime_error(
            std::string{"Failed to select GPU: "} +
            phys_result.error().message()
        );
    }

    physical_device = phys_result.value().physical_device;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device, &props);
    std::cout << "GPU: " << props.deviceName << "\n";
    std::cout << "Vulkan: "
                << VK_VERSION_MAJOR(props.apiVersion) << "."
                << VK_VERSION_MINOR(props.apiVersion) << "."
                << VK_VERSION_PATCH(props.apiVersion) << "\n";


    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.synchronization2 = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;

    auto device_result = vkb::DeviceBuilder{phys_result.value()}
        .add_pNext(&features12)
        .add_pNext(&features13)
        .build();

    if (!device_result) {
        throw std::runtime_error(
            std::string{"Failed to create device: "} +
            device_result.error().message()
        );
    }

    vkb_device = device_result.value();
    device = vkb_device.device;


    auto queue_result = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!queue_result) {
        throw std::runtime_error("Failed to get graphics queue");
    }

    graphics_queue = queue_result.value();
    graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physical_device;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator);

    mainDeletionQueue.push_function([&](){
        vmaDestroyAllocator(allocator);
    });

    pushConstants.data1 = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);  
    pushConstants.data2 = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);  
    init_vulkan();
}

VulkanContext::~VulkanContext(){
    
    if(device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device);
    

    for(auto frame : frames){
        vkDestroyCommandPool(device,frame.commandPool, nullptr);
        vkDestroyFence(device, frame.renderFence, nullptr);
        vkDestroySemaphore(device, frame.renderSemaphore, nullptr);
        vkDestroySemaphore(device, frame.swapchainSemaphore, nullptr);

        frame.deletionQueue.flush();
    }
    
    mainDeletionQueue.flush();

    destroy_swapchain();



    if(surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(instance, surface, nullptr);

    vkb::destroy_device(vkb_device);
    vkb::destroy_instance(vkb_instance);
}

void VulkanContext::create_swapchain(uint32_t width, uint32_t height){
    
    vkb::SwapchainBuilder swapchainBuilder{vkb_device, surface};

    swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    auto swap_result = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{
            .format = swapchainImageFormat,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        })
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build();

    if (!swap_result) {
        throw std::runtime_error(
            std::string{"Failed to create swapchain: "} +
            swap_result.error().message()
        );
    }

    auto vkbSwapchain = swap_result.value();
    swapchainExtent = vkbSwapchain.extent;
	swapchain = vkbSwapchain.swapchain;
	swapchainImages = vkbSwapchain.get_images().value();
	swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanContext::init_swapchain(){
    int w, h;
    glfwGetFramebufferSize(glfwWindow, &w, &h);
    create_swapchain(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    VkExtent3D drawImageExtent = {
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        1
    };

    drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = image_create_info(drawImage.imageFormat, drawImageUsages, drawImageExtent);

    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);


    vmaCreateImage(allocator, &rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation, nullptr);
    VkImageViewCreateInfo rview_info = imageview_create_info(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &drawImage.imageView));

	mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(device, drawImage.imageView, nullptr);
		vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);
    });
}

void VulkanContext::destroy_swapchain(){
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    for(auto view : swapchainImageViews)
        vkDestroyImageView(device, view, nullptr);
}

void VulkanContext::destroy_buffer(const AllocatedBuffer& buffer) {
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

void VulkanContext::init_commands(){
    
    VkCommandPoolCreateInfo commandPoolInfo =  {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.pNext = nullptr;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = graphics_queue_family;
	
	for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].commandPool));

		VkCommandBufferAllocateInfo cmdAllocInfo = {};
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.pNext = nullptr;
		cmdAllocInfo.commandPool = frames[i].commandPool;
		cmdAllocInfo.commandBufferCount = 1;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].mainCommandBuffer));
	}

    VkCommandPoolCreateInfo immPoolInfo{};
    immPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    immPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    immPoolInfo.queueFamilyIndex = graphics_queue_family;

    VK_CHECK(vkCreateCommandPool(device, &immPoolInfo, nullptr, &immCommandPool));

    VkCommandBufferAllocateInfo immAllocInfo{};
    immAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    immAllocInfo.commandPool = immCommandPool;
    immAllocInfo.commandBufferCount = 1;
    immAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_CHECK(vkAllocateCommandBuffers(device, &immAllocInfo, &immCommandBuffer));

    mainDeletionQueue.push_function([=]() {
        vkDestroyCommandPool(device, immCommandPool, nullptr);
    });

}


VkFenceCreateInfo VulkanContext::fence_create_info(VkFenceCreateFlags flags){
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = nullptr;

    info.flags = flags;

    return info;
}
VkSemaphoreCreateInfo VulkanContext::semaphore_create_info(VkSemaphoreCreateFlags flags){
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

VkCommandBufferBeginInfo VulkanContext::command_buffer_begin_info(VkCommandBufferUsageFlags flags){
    
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;

    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}
VkSemaphoreSubmitInfo VulkanContext::semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
	VkSemaphoreSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.semaphore = semaphore;
	submitInfo.stageMask = stageMask;
	submitInfo.deviceIndex = 0;
	submitInfo.value = 1;

	return submitInfo;
}

VkCommandBufferSubmitInfo VulkanContext::command_buffer_submit_info(VkCommandBuffer cmd)
{
	VkCommandBufferSubmitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = nullptr;
	info.commandBuffer = cmd;
	info.deviceMask = 0;

	return info;
}

VkSubmitInfo2 VulkanContext::submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
    VkSemaphoreSubmitInfo* waitSemaphoreInfo)
{
    VkSubmitInfo2 info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    info.pNext = nullptr;

    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmd;

    return info;
}

VkImageCreateInfo VulkanContext::image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent){
    
    VkImageCreateInfo info = {};
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
VkImageViewCreateInfo VulkanContext::imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags){

    VkImageViewCreateInfo info = {};
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

AllocatedBuffer VulkanContext::create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                             VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaalocationInfo = {};
    vmaalocationInfo.usage = memoryUsage;
    vmaalocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaalocationInfo, &newBuffer.buffer,
                             &newBuffer.allocation, &newBuffer.info));



    return newBuffer;
}

GPUMeshBuffers VulkanContext::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
    
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    newSurface.vertexBuffer =
        create_buffer(vertexBufferSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo deviceAdressInfo{.sType =
                                                   VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                               .buffer = newSurface.vertexBuffer.buffer};
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(device, &deviceAdressInfo);

    newSurface.indexBuffer = create_buffer(
        indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    
    	AllocatedBuffer staging =
        create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.info.pMappedData;
    
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{0};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    destroy_buffer(staging);

    return newSurface;
}

void VulkanContext::init_sync_structures(){

    VkFenceCreateInfo fenceCreateInfo = fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));

		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].renderSemaphore));
	}

    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &immFence));
    mainDeletionQueue.push_function([=](){
        vkDestroyFence(device, immFence, nullptr);
    });
}
void VulkanContext::draw() {

    VK_CHECK(vkWaitForFences(device, 1, &get_current_frame().renderFence, true, 1000000000));
    get_current_frame().deletionQueue.flush();
    VK_CHECK(vkResetFences(device, 1, &get_current_frame().renderFence));

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("background")) {
        ImGui::Text("Frame: %d", frameNumber);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Separator();
        ImGui::ColorEdit4("Top Color", &pushConstants.data1.x);
        ImGui::ColorEdit4("Bottom Color", &pushConstants.data2.x);
    }
    ImGui::End();
    ImGui::Render();
    int wx, wy, fx, fy;
    glfwGetWindowSize(glfwWindow, &wx, &wy);
    glfwGetFramebufferSize(glfwWindow, &fx, &fy);
    const ImGuiIO& io = ImGui::GetIO();
    std::printf("win %dx%d | fb %dx%d | swap %ux%u | display %.0fx%.0f | scale %.2fx%.2f\n",
        wx, wy, fx, fy,
        swapchainExtent.width, swapchainExtent.height,
        io.DisplaySize.x, io.DisplaySize.y,
        io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 1000000000,
                                   get_current_frame().swapchainSemaphore, VK_NULL_HANDLE,
                                   &swapchainImageIndex));

    VkCommandBuffer cmd = get_current_frame().mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo cmdBeginInfo = command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
        
    drawExtent.width  = std::min(swapchainExtent.width,  drawImage.imageExtent.width);
    drawExtent.height = std::min(swapchainExtent.height, drawImage.imageExtent.height);

    transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    draw_background(cmd);

    transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    draw_geometry(cmd);


    transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_image_to_image(cmd, drawImage.image, swapchainImages[swapchainImageIndex], drawImage.imageExtent, swapchainExtent);

    transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = swapchainImageViews[swapchainImageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = { {0, 0}, swapchainExtent };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);

    transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = command_buffer_submit_info(cmd);
    VkSemaphoreSubmitInfo waitInfo = semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().renderSemaphore);
    VkSubmitInfo2 submit = submit_info(&cmdinfo, &signalInfo, &waitInfo);

    VK_CHECK(vkQueueSubmit2(graphics_queue, 1, &submit, get_current_frame().renderFence));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &get_current_frame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(graphics_queue, &presentInfo));
    frameNumber++;
}

void VulkanContext::draw_background(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout,
        0, 1, &drawImageDescriptors, 0, nullptr);

    vkCmdPushConstants(cmd, gradientPipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(ComputePushConstants),
        &pushConstants);

    vkCmdDispatch(cmd,
        static_cast<uint32_t>(std::ceil(drawImage.imageExtent.width / 16.0)),
        static_cast<uint32_t>(std::ceil(drawImage.imageExtent.height / 16.0)),
        1);
}

void VulkanContext::init_descriptors() {
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };
    globalDescriptorAllocator.init_pool(device, 10, sizes);

    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);

    drawImageDescriptors = globalDescriptorAllocator.allocate(device, drawImageDescriptorLayout);

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = drawImage.imageView;

    VkWriteDescriptorSet drawImageWrite{};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = drawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(device, 1, &drawImageWrite, 0, nullptr);

    mainDeletionQueue.push_function([&]() {
        globalDescriptorAllocator.destroy_pool(device);
        vkDestroyDescriptorSetLayout(device, drawImageDescriptorLayout, nullptr);
    });
}

void VulkanContext::init_pipelines() {
    init_background_pipelines();
    init_triangle_pipeline();
    init_mesh_pipeline();
}

void VulkanContext::init_background_pipelines() {

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ComputePushConstants);

    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pSetLayouts = &drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;
    computeLayout.pPushConstantRanges = &pushConstantRange;   
    computeLayout.pushConstantRangeCount = 1;                  

    VK_CHECK(vkCreatePipelineLayout(device, &computeLayout, nullptr, &gradientPipelineLayout));

    VkShaderModule computeDrawShader;
    if (!load_shader_module("shaders/gradient_color.comp.spv", device, &computeDrawShader)) {
        throw std::runtime_error("Failed to load compute shader");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeDrawShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.layout = gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradientPipeline));

    vkDestroyShaderModule(device, computeDrawShader, nullptr);

    mainDeletionQueue.push_function([=]() {
        vkDestroyPipelineLayout(device, gradientPipelineLayout, nullptr);
        vkDestroyPipeline(device, gradientPipeline, nullptr);
    });
}

void VulkanContext::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VK_CHECK(vkResetFences(device, 1, &immFence));
	VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

	VkCommandBuffer cmd = immCommandBuffer;

	VkCommandBufferBeginInfo cmdBeginInfo = command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = command_buffer_submit_info(cmd);
	VkSubmitInfo2 submit = submit_info(&cmdinfo, nullptr, nullptr);


	VK_CHECK(vkQueueSubmit2(graphics_queue, 1, &submit, immFence));

	VK_CHECK(vkWaitForFences(device, 1, &immFence, true, 9999999999));
}

void VulkanContext::init_imgui() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool));

    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physical_device;
    init_info.Device = device;
    init_info.Queue = graphics_queue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = static_cast<uint32_t>(swapchainImages.size());
    init_info.ImageCount    = FRAME_OVERLAP;
    init_info.UseDynamicRendering = true;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat;

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();

    mainDeletionQueue.push_function([=]() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(device, imguiPool, nullptr);
    });
}

bool VulkanContext::load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return false;

    auto fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return false;
    }
    *outShaderModule = shaderModule;
    return true;
}

void VulkanContext::init_triangle_pipeline(){

    VkShaderModule triangleFragShader;
    if(!load_shader_module("shaders/colored_triangle.frag.spv", device, &triangleFragShader))
        std::fprintf(stderr, "Error when building the triangle fragment shader module\n");
    else
        std::fprintf(stderr, "Triangle fragment shader succesfully loaded\n");

    VkShaderModule triangleVertexShader;
    if(!load_shader_module("shaders/colored_triangle.vert.spv",device, &triangleVertexShader))
        std::fprintf(stderr, "Error when building the triangle vertex shader module\n");
    else
        std::fprintf(stderr, "Triangle vertex shader succesfully loaded\n");

    VkPipelineLayoutCreateInfo pipeline_layout_info = pipeline_layout_create_info();
	VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &trianglePipelineLayout));



    PipelineBuilder pipelineBuilder;

	pipelineBuilder.pipelineLayout = trianglePipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.set_multisampling_none();
	//no blending
	pipelineBuilder.disable_blending();
	//no depth testing
	pipelineBuilder.disable_depthtest();

	//connect the image format we will draw into, from draw image
	pipelineBuilder.set_color_attachment_format(drawImage.imageFormat);
	pipelineBuilder.set_depth_format(VK_FORMAT_UNDEFINED);

	//finally build the pipeline
	trianglePipeline = pipelineBuilder.build_pipeline(device);

	//clean structures
	vkDestroyShaderModule(device, triangleFragShader, nullptr);
	vkDestroyShaderModule(device, triangleVertexShader, nullptr);

	mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(device, trianglePipelineLayout, nullptr);
		vkDestroyPipeline(device, trianglePipeline, nullptr);
	});

}
void VulkanContext::init_mesh_pipeline() {
    VkShaderModule fragmentShader;
    if (!load_shader_module("shaders/colored_triangle.frag.spv", device, &fragmentShader))
        std::fprintf(stderr, "Cannot load triangle mesh fragment shader\n");
    else
        std::fprintf(stderr, "Triangle mesh fragment shader loaded\n");

    VkShaderModule vertexShader;
    if (!load_shader_module("shaders/colored_triangle_mesh.vert.spv", device, &vertexShader))
        std::fprintf(stderr, "Cannot load triangle mesh vertex shader\n");
    else
        std::fprintf(stderr, "Triangle mesh vertex shader loaded\n");

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &bufferRange;
    pipeline_layout_info.pushConstantRangeCount = 1;
    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &meshPipelineLayout));

    PipelineBuilder pipelineBuilder;

    pipelineBuilder.pipelineLayout = meshPipelineLayout;
    pipelineBuilder.set_shaders(vertexShader, fragmentShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.disable_depthtest();
    pipelineBuilder.set_color_attachment_format(drawImage.imageFormat);
    pipelineBuilder.set_depth_format(VK_FORMAT_UNDEFINED);

    meshPipeline = pipelineBuilder.build_pipeline(device);
    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    mainDeletionQueue.push_function([=]() { 
            vkDestroyPipelineLayout(device, meshPipelineLayout, nullptr);
            vkDestroyPipeline(device, meshPipeline, nullptr);
    });
}
void VulkanContext::init_default_data() {
    std::array<Vertex, 4> rect_vertices;

    rect_vertices[0].position = {0.5, -0.5, 0};
    rect_vertices[1].position = {0.5, 0.5, 0};
    rect_vertices[2].position = {-0.5, -0.5, 0};
    rect_vertices[3].position = {-0.5, 0.5, 0};

    rect_vertices[0].color = {0, 0, 0, 1};
    rect_vertices[1].color = {0.5, 0.5, 0.5, 1};
    rect_vertices[2].color = {1, 0, 0, 1};
    rect_vertices[3].color = {0, 1, 0, 1};

    std::array<uint32_t, 6> rect_indices;

    rect_indices[0] = 0;
    rect_indices[1] = 1;
    rect_indices[2] = 2;

    rect_indices[3] = 2;
    rect_indices[4] = 1;
    rect_indices[5] = 3;

    rectangle = uploadMesh(rect_indices, rect_vertices);

    mainDeletionQueue.push_function([&]() {
        destroy_buffer(rectangle.indexBuffer);
        destroy_buffer(rectangle.vertexBuffer);
    });
}
void VulkanContext::draw_geometry(VkCommandBuffer cmd){
    
    //begin a render pass  connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = attachment_info(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderInfo = rendering_info(drawExtent, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = drawExtent.width;
	viewport.height = drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = drawExtent.width;
	scissor.extent.height = drawExtent.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);

    GPUDrawPushConstants push_constants;
    push_constants.worldMatrix = glm::mat4{1.f};
    push_constants.vertexBuffer = rectangle.vertexBufferAddress;

    vkCmdPushConstants(cmd, meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                        sizeof(GPUDrawPushConstants), &push_constants);
    vkCmdBindIndexBuffer(cmd, rectangle.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

	vkCmdEndRendering(cmd);

}

