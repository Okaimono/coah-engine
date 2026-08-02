#pragma once
#include "vulkan_includes.hpp"
#include "vulkan_context.hpp"
#include "vulkan/command_manager.hpp"
#include "stb_image.h"

#include <string>
#include <stdexcept>
#include <cstring>

struct GpuTexture {
    VkImage        image   = VK_NULL_HANDLE;
    VkDeviceMemory memory  = VK_NULL_HANDLE;
    VkImageView    view    = VK_NULL_HANDLE;
    VkSampler      sampler = VK_NULL_HANDLE;
    uint32_t       width   = 0;
    uint32_t       height  = 0;
};

class TextureManager {
public:
    TextureManager(VulkanContext& ctx, CommandManager& commandManager)
        : ctx_(ctx), commandManager_(commandManager) {}

    // Loads a PNG from disk, uploads it to GPU-resident memory, and returns
    // everything needed to sample it (image, view, sampler, dimensions).
    // format lets callers pick SRGB (world textures) vs UNORM (UI/icons) —
    // your existing code used SRGB for the chunk atlas, UNORM for the UI atlas.
    GpuTexture load(const std::string& path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB) {
        int w, h, channels;
        stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
        if (!pixels) {
            throw std::runtime_error("TextureManager: failed to load texture: " + path);
        }

        VkDeviceSize imageSize = (VkDeviceSize)w * h * 4;

        GpuTexture tex;
        tex.width  = (uint32_t)w;
        tex.height = (uint32_t)h;

        VkBuffer       stagingBuffer;
        VkDeviceMemory stagingMemory;
        createBuffer(imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingMemory);

        void* data;
        vkMapMemory(ctx_.device, stagingMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, (size_t)imageSize);
        vkUnmapMemory(ctx_.device, stagingMemory);
        stbi_image_free(pixels);

        createImage(tex.width, tex.height, format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            tex.image, tex.memory);

        transitionImageLayout(tex.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, tex.image, tex.width, tex.height);
        transitionImageLayout(tex.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(ctx_.device, stagingBuffer, nullptr);
        vkFreeMemory(ctx_.device, stagingMemory, nullptr);

        createImageView(tex.image, format, tex.view);
        createSampler(tex.sampler);

        return tex;
    }

    // Call to release a texture's GPU resources (e.g. on hot-reload, or engine shutdown
    // if you're not just letting process exit clean everything up).
    void destroy(GpuTexture& tex) {
        if (tex.sampler != VK_NULL_HANDLE) vkDestroySampler(ctx_.device, tex.sampler, nullptr);
        if (tex.view    != VK_NULL_HANDLE) vkDestroyImageView(ctx_.device, tex.view, nullptr);
        if (tex.image   != VK_NULL_HANDLE) vkDestroyImage(ctx_.device, tex.image, nullptr);
        if (tex.memory  != VK_NULL_HANDLE) vkFreeMemory(ctx_.device, tex.memory, nullptr);
        tex = GpuTexture{};
    }

private:
    VulkanContext&  ctx_;
    CommandManager& commandManager_;

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

    void createImage(uint32_t width, uint32_t height, VkFormat format,
                      VkImageTiling tiling, VkImageUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkImage& image, VkDeviceMemory& memory) {
        VkImageCreateInfo info{};
        info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType     = VK_IMAGE_TYPE_2D;
        info.extent        = {width, height, 1};
        info.mipLevels     = 1;
        info.arrayLayers   = 1;
        info.format        = format;
        info.tiling        = tiling;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.usage         = usage;
        info.samples       = VK_SAMPLE_COUNT_1_BIT;
        info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateImage(ctx_.device, &info, nullptr, &image);

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(ctx_.device, image, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits, properties);
        vkAllocateMemory(ctx_.device, &allocInfo, nullptr, &memory);
        vkBindImageMemory(ctx_.device, image, memory, 0);
    }

    void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer cmd = commandManager_.beginOneShot();

        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = oldLayout;
        barrier.newLayout           = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage, dstStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        commandManager_.endOneShot(cmd);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t w, uint32_t h) {
        VkCommandBuffer cmd = commandManager_.beginOneShot();

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent                 = {w, h, 1};

        vkCmdCopyBufferToImage(cmd, buffer, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        commandManager_.endOneShot(cmd);
    }

    void createImageView(VkImage image, VkFormat format, VkImageView& view) {
        VkImageViewCreateInfo info{};
        info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image    = image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format   = format;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        vkCreateImageView(ctx_.device, &info, nullptr, &view);
    }

    void createSampler(VkSampler& sampler) {
        VkSamplerCreateInfo info{};
        info.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter    = VK_FILTER_NEAREST;
        info.minFilter    = VK_FILTER_NEAREST;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(ctx_.device, &info, nullptr, &sampler);
    }
};