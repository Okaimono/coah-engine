#pragma once
#include "vulkan_includes.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/command_manager.hpp"

#include <cstdint>

struct Slot {
    static constexpr VkDeviceSize slotSize = sizeof(uint32_t) * 50000;
    VkDeviceSize slotOffset = -1;
};

class PoolAllocator {
public:
    static constexpr VkDeviceSize poolSize = 256 << 20;

    VkBuffer ssboBuffer = VK_NULL_HANDLE;
    VkDeviceMemory ssboMemory = VK_NULL_HANDLE;

    PoolAllocator(VulkanContext& ctx, CommandManager& commandManager)
        : ctx_(ctx)
        , commandManager_(commandManager)
    {
        createSSBO();
    }

    ~PoolAllocator() {
        if (ssboBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx_.device, ssboBuffer, nullptr);
        }
        if (ssboMemory != VK_NULL_HANDLE) {
            vkFreeMemory(ctx_.device, ssboMemory, nullptr);
        }
    }

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    Slot reserveSlot() {
        Slot slot;
        slot.slotOffset = ssboOffset;
        ssboOffset += slot.slotSize;
        return slot;
    }

    void updateSlot(Slot& slot, const std::vector<uint32_t>& slotData) {
        std::vector<uint32_t> newData;
        newData.resize(50000, 0);
        for (int i = 0; i < slotData.size(); i++) {
            newData[i] = slotData[i];
        }

        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

        const VkDeviceSize stagingPoolSize = newData.size() * sizeof(uint32_t);
        createBuffer(stagingPoolSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingMemory);

        void* data;
        vkMapMemory(ctx_.device, stagingMemory, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy(data, newData.data(), newData.size() * sizeof(uint32_t));
        vkUnmapMemory(ctx_.device, stagingMemory);
        VkCommandBuffer cmd = commandManager_.beginOneShot();
        VkBufferCopy copy{};
        copy.srcOffset = 0;
        copy.dstOffset = slot.slotOffset;
        copy.size = newData.size() * sizeof(uint32_t);
        vkCmdCopyBuffer(cmd, stagingBuffer, ssboBuffer, 1, &copy);
        commandManager_.endOneShot(cmd);

        vkDestroyBuffer(ctx_.device, stagingBuffer, nullptr);
        vkFreeMemory(ctx_.device, stagingMemory, nullptr);
    }

private:
    VulkanContext& ctx_;
    CommandManager& commandManager_;

    VkDeviceSize ssboOffset = 0;

    void createSSBO() {
        createBuffer(poolSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            ssboBuffer, ssboMemory);
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo info{};
        info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size        = size;
        info.usage       = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(ctx_.device, &info, nullptr, &buffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(ctx_.device, buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits, properties);
        vkAllocateMemory(ctx_.device, &allocInfo, nullptr, &memory);
        vkBindBufferMemory(ctx_.device, buffer, memory, 0);
    }
};