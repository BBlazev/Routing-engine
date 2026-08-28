#include <vulkan/vulkan_context.hpp>
#include <vulkan/vk_check.hpp>
#include <settings.hpp>
#include <iostream>


void VulkanContext::init_vulkan(){
    init_swapchain();
    init_commands();

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


    auto device_result = vkb::DeviceBuilder{phys_result.value()}
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

}

VulkanContext::~VulkanContext(){
    
    if(device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device);
    

    for(auto frame : frames)
        vkDestroyCommandPool(device,frame.commandPool, nullptr);
    
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
	
	for (int i = 0; i < FRAME_OVERLAP; i++) {

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