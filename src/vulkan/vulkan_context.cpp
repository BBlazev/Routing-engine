#include <vulkan/vulkan_context.hpp>
#include <vulkan/vk_check.hpp>
#include <vulkan/vk_image.hpp>
#include <settings.hpp>
#include <iostream>
#include <cmath>


void VulkanContext::init_vulkan(){
    init_swapchain();
    init_commands();
    init_sync_structures();



}


VulkanContext::VulkanContext(GLFWwindow* window){

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
    create_swapchain(SCREEN_WIDTH, SCREEN_HEIGHT);

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

void VulkanContext::init_sync_structures(){

    VkFenceCreateInfo fenceCreateInfo = fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));

		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].renderSemaphore));
	}
}
void VulkanContext::draw(){

	VK_CHECK(vkWaitForFences(device, 1, &get_current_frame().renderFence, true, 1000000000));
	
    get_current_frame().deletionQueue.flush();
    
    VK_CHECK(vkResetFences(device, 1, &get_current_frame().renderFence));

    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 1000000000, get_current_frame().swapchainSemaphore, nullptr, &swapchainImageIndex));

    VkCommandBuffer cmd = get_current_frame().mainCommandBuffer;

    VK_CHECK(vkResetCommandBuffer(cmd,0));

    VkCommandBufferBeginInfo cmdBeginInfo = command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(cmd);

    transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copy_image_to_image(cmd, drawImage.image, swapchainImages[swapchainImageIndex], drawImage.imageExtent, swapchainExtent);

    transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = command_buffer_submit_info(cmd);	
	
	VkSemaphoreSubmitInfo waitInfo = semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,get_current_frame().swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().renderSemaphore);	
	
	VkSubmitInfo2 submit = submit_info(&cmdinfo,&signalInfo,&waitInfo);	

	VK_CHECK(vkQueueSubmit2(graphics_queue, 1, &submit, get_current_frame().renderFence));

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &get_current_frame().renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(graphics_queue, &presentInfo));

	frameNumber++;
}

void VulkanContext::draw_background(VkCommandBuffer cmd){
    
    VkClearColorValue clearValue;
	float flash = std::abs(std::sin(frameNumber / 120.f));
	clearValue = { { 0.0f, 0.0f, flash, 1.0f } };
	VkImageSubresourceRange clearRange = image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdClearColorImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
}