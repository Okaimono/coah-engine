#pragma once
#include "vulkan_context.hpp"
#include <vector>
#include <stdexcept>

class CommandManager {
public:
    VkCommandPool commandPool = VK_NULL_HANDLE;

    CommandManager(VulkanContext& ctx)
        : ctx_(ctx)
    {
        createCommandPool();
    }

    ~CommandManager() {
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(ctx_.device, commandPool, nullptr);
        }
    }

    CommandManager(const CommandManager&)            = delete;
    CommandManager& operator=(const CommandManager&) = delete;
    CommandManager(CommandManager&&)                 = delete;
    CommandManager& operator=(CommandManager&&)      = delete;

    VkCommandBuffer beginOneShot() {
        VkCommandBufferAllocateInfo info{};
        info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool        = commandPool;
        info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(ctx_.device, &info, &cmd);

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);
        return cmd;
    }

    void endOneShot(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;
        vkQueueSubmit(ctx_.graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(ctx_.graphicsQueue);
        vkFreeCommandBuffers(ctx_.device, commandPool, 1, &cmd);
    }

private:
    VulkanContext& ctx_;

    void createCommandPool() {
        VkCommandPoolCreateInfo info{};
        info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.queueFamilyIndex = getGraphicsFamily();
        info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(ctx_.device, &info, nullptr, &commandPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create command pool");
    }

    uint32_t getGraphicsFamily() {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(ctx_.physicalDevice, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(ctx_.physicalDevice, &count, families.data());
        for (uint32_t i = 0; i < count; i++)
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                return i;
        throw std::runtime_error("no graphics queue family");
    }
};