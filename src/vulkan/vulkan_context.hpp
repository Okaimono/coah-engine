#pragma once
#include "vulkan_includes.hpp"
#include "vulkan/window.hpp"

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

class VulkanContext {
public:
    VkInstance   instance       = VK_NULL_HANDLE;
    VkSurfaceKHR surface        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice     device         = VK_NULL_HANDLE;
    VkQueue      graphicsQueue  = VK_NULL_HANDLE;

    VulkanContext(Window& window) {
        createInstance();
        createSurface(window);
        pickPhysicalDevice();
        createLogicalDevice();
    }

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
            if ((typeFilter & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        throw std::runtime_error("failed to find suitable memory type");
    }

private:
    const bool useDiscreteGPU = true;

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayers())
            throw std::runtime_error("validation layers not available");

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "coah-engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        uint32_t    glfwExtCount = 0;
        const char** glfwExts   = glfwGetRequiredInstanceExtensions(&glfwExtCount);

        VkInstanceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo = &appInfo;
        info.enabledExtensionCount = glfwExtCount;
        info.ppEnabledExtensionNames = glfwExts;
        if (enableValidationLayers) {
            info.enabledLayerCount   = validationLayers.size();
            info.ppEnabledLayerNames = validationLayers.data();
        }

        if (vkCreateInstance(&info, nullptr, &instance) != VK_SUCCESS)
            throw std::runtime_error("failed to create instance");
        std::cout << "instance created\n";
    }

    void createSurface(Window& window) {
        if (glfwCreateWindowSurface(instance, window.get(), nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("failed to create surface");
    }

    void pickPhysicalDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (count == 0) throw std::runtime_error("no GPUs with Vulkan support");

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());

        VkPhysicalDeviceType preferred = useDiscreteGPU 
            ? VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 
            : VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

        for (auto& d : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(d, &props);
            std::cout << "found: " << props.deviceName << "\n";
            if (props.deviceType == preferred) {
                physicalDevice = d;
                std::cout << "selected: " << props.deviceName << "\n";
                return;
            }
        }

        // fallback to whatever's available
        physicalDevice = devices[0];
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        std::cout << "fallback selected: " << props.deviceName << "\n";
    }

    void createLogicalDevice() {
        uint32_t gfx     = getGraphicsFamily();
        float    priority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = gfx;
        queueInfo.queueCount       = 1;
        queueInfo.pQueuePriorities = &priority;

        VkDeviceCreateInfo info{};
        info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.queueCreateInfoCount    = 1;
        info.pQueueCreateInfos       = &queueInfo;
        info.enabledExtensionCount   = deviceExtensions.size();
        info.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(physicalDevice, &info, nullptr, &device) != VK_SUCCESS)
            throw std::runtime_error("failed to create logical device");

        vkGetDeviceQueue(device, gfx, 0, &graphicsQueue);
        std::cout << "logical device created\n";
    }

    bool checkValidationLayers() {
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> available(count);
        vkEnumerateInstanceLayerProperties(&count, available.data());
        for (const char* name : validationLayers) {
            bool found = false;
            for (auto& layer : available)
                if (strcmp(name, layer.layerName) == 0) { found = true; break; }
            if (!found) return false;
        }
        return true;
    }

    uint32_t getGraphicsFamily() {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, families.data());
        for (uint32_t i = 0; i < count; i++)
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                return i;
        throw std::runtime_error("no graphics queue family");
    }

    void cleanup() {
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
};