#include <vulkan/vulkan_context.hpp>
#include <iostream>

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

    if(surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(instance, surface, nullptr);

    vkb::destroy_device(vkb_device);
    vkb::destroy_instance(vkb_instance);
}