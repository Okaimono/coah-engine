#pragma once
#include "stb_image.h"
#include "vulkan_context.hpp"
#include "swapchain.hpp"
#include "vulkan/renderer/render_pass.hpp"
#include "vulkan/renderer/descriptor_set_layout.hpp"
#include "vulkan/renderer/chunk_pipeline.hpp"
#include "vulkan/renderer/ui_pipeline.hpp"
#include "core/types.hpp"
#include "core/config.hpp"
#include "vulkan/command_manager.hpp"
#include "vulkan/pool_allocator.hpp"
#include "game/world.hpp"

#include <functional>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>

class Renderer {
public:
    Renderer(VulkanContext& ctx, Swapchain& swapchain)
        : ctx_(ctx)
        , swapchain_(swapchain)
        , renderPass_(ctx.device, swapchain.swapchainFormat)
        , allocator_(ctx, commandManager_)
        , descriptorSetLayout_(ctx)
        , chunkPipeline_(ctx, swapchain, renderPass_, descriptorSetLayout_.handle)
        , uiPipeline_(ctx, swapchain, renderPass_)
    {
        createDepthResources();
        createFramebuffers();
        commandManager_.init(&ctx);
        createTextureImage();
        createTextureImageView();
        createSampler();
        createUniformBuffer();
        createDescriptorPool();
        createDescriptorSet();
        createCommandBuffers();
        createSyncObjects();
    }

    Slot reserveChunkSlot(const std::vector<uint32_t>& faceData) {
        Slot slot = allocator_.reserveSlot();
        allocator_.updateSlot(slot, faceData);
        return slot;
    }

    void updateChunkSlot(Slot& slot, const std::vector<uint32_t>& faceData) {
        allocator_.updateSlot(slot, faceData);
    }

    void updateUI(const std::vector<UIVertex>& vertices) {
        uiPipeline_.updateVertexBuffer(vertices);
    }

    void updateUniformBuffer(const glm::mat4& view, const glm::mat4& proj) {
        UniformBufferObject ubo{};
        ubo.view = view;
        ubo.proj = proj;
        memcpy(uniformMapped_, &ubo, sizeof(ubo));
    }

    void drawFrame(World& world) {
        vkWaitForFences(ctx_.device, 1, &inFlight_, VK_TRUE, UINT64_MAX);
        vkResetFences(ctx_.device, 1, &inFlight_);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(ctx_.device, swapchain_.swapchain, UINT64_MAX, imageAvailable_, VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(commandBuffers_[imageIndex], 0);
        recordCommandBuffer(commandBuffers_[imageIndex], imageIndex, world);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit{};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount   = 1;
        submit.pWaitSemaphores      = &imageAvailable_;
        submit.pWaitDstStageMask    = &waitStage;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &commandBuffers_[imageIndex];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores    = &renderFinished_;
        vkQueueSubmit(ctx_.graphicsQueue, 1, &submit, inFlight_);

        VkPresentInfoKHR present{};
        present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores    = &renderFinished_;
        present.swapchainCount     = 1;
        present.pSwapchains        = &swapchain_.swapchain;
        present.pImageIndices      = &imageIndex;
        vkQueuePresentKHR(ctx_.graphicsQueue, &present);
    }

    ~Renderer() {
        vkDestroySampler(ctx_.device, textureSampler_, nullptr);
        vkDestroyImageView(ctx_.device, textureImageView_, nullptr);
        vkDestroyImage(ctx_.device, textureImage_, nullptr);
        vkFreeMemory(ctx_.device, textureMemory_, nullptr);
        vkDestroyImageView(ctx_.device, depthImageView_, nullptr);
        vkDestroyImage(ctx_.device, depthImage_, nullptr);
        vkFreeMemory(ctx_.device, depthMemory_, nullptr);
        vkDestroyBuffer(ctx_.device, uniformBuffer_, nullptr);
        vkFreeMemory(ctx_.device, uniformMemory_, nullptr);
        vkDestroyDescriptorPool(ctx_.device, descriptorPool_, nullptr);
        vkDestroySemaphore(ctx_.device, imageAvailable_, nullptr);
        vkDestroySemaphore(ctx_.device, renderFinished_, nullptr);
        vkDestroyFence(ctx_.device, inFlight_, nullptr);
        commandManager_.cleanup();
        for (auto fb : framebuffers_) vkDestroyFramebuffer(ctx_.device, fb, nullptr);
    }

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

private:
    VulkanContext& ctx_;
    Swapchain&     swapchain_;

    RenderPass     renderPass_;
    CommandManager commandManager_;
    PoolAllocator  allocator_;

    DescriptorSetLayout descriptorSetLayout_;
    ChunkPipeline        chunkPipeline_;
    UiPipeline            uiPipeline_;

    std::vector<VkFramebuffer>   framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;

    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;
    VkFence     inFlight_       = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet  descriptorSet_  = VK_NULL_HANDLE;

    VkBuffer       uniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniformMemory_ = VK_NULL_HANDLE;
    void*          uniformMapped_ = nullptr;

    VkImage        depthImage_     = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_    = VK_NULL_HANDLE;
    VkImageView    depthImageView_ = VK_NULL_HANDLE;

    VkImage        textureImage_     = VK_NULL_HANDLE;
    VkDeviceMemory textureMemory_    = VK_NULL_HANDLE;
    VkImageView    textureImageView_ = VK_NULL_HANDLE;
    VkSampler      textureSampler_   = VK_NULL_HANDLE;

    uint32_t currentImageIndex_ = 0;

    void createDescriptorPool() {
        VkDescriptorPoolSize poolSizes[3]{};
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = 1;
        poolSizes[2].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[2].descriptorCount = 1;

        VkDescriptorPoolCreateInfo info{};
        info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.poolSizeCount = 3;
        info.pPoolSizes    = poolSizes;
        info.maxSets       = 1;
        vkCreateDescriptorPool(ctx_.device, &info, nullptr, &descriptorPool_);
    }

    void createDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = descriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &descriptorSetLayout_.handle;
        vkAllocateDescriptorSets(ctx_.device, &allocInfo, &descriptorSet_);

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = uniformBuffer_;
        uboInfo.offset = 0;
        uboInfo.range  = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo ssboInfo{};
        ssboInfo.buffer = allocator_.ssboBuffer;
        ssboInfo.offset = 0;
        ssboInfo.range  = allocator_.poolSize;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = textureImageView_;
        imageInfo.sampler     = textureSampler_;

        VkWriteDescriptorSet writes[3]{};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = descriptorSet_;
        writes[0].dstBinding      = 0;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo     = &uboInfo;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = descriptorSet_;
        writes[1].dstBinding      = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo     = &ssboInfo;

        writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet          = descriptorSet_;
        writes[2].dstBinding      = 2;
        writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(ctx_.device, 3, writes, 0, nullptr);
    }

    // ─────────────────────────────────────────
    //  Framebuffers / Commands / Sync
    // ─────────────────────────────────────────

    void createFramebuffers() {
        framebuffers_.resize(swapchain_.imageViews.size());
        for (size_t i = 0; i < swapchain_.imageViews.size(); i++) {
            VkImageView attachments[] = {swapchain_.imageViews[i], depthImageView_};
            VkFramebufferCreateInfo info{};
            info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass      = renderPass_.handle();
            info.attachmentCount = 2;
            info.pAttachments    = attachments;
            info.width           = swapchain_.swapchainExtent.width;
            info.height          = swapchain_.swapchainExtent.height;
            info.layers          = 1;
            if (vkCreateFramebuffer(ctx_.device, &info, nullptr, &framebuffers_[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create framebuffer");
        }
    }

    void createCommandBuffers() {
        commandBuffers_.resize(framebuffers_.size());
        VkCommandBufferAllocateInfo info{};
        info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool        = commandManager_.commandPool;
        info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = (uint32_t)commandBuffers_.size();
        if (vkAllocateCommandBuffers(ctx_.device, &info, commandBuffers_.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate command buffers");
    }

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, World& world) {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &begin);

        VkClearValue clearValues[2]{};
        clearValues[0].color        = {0.0f, 0.0f, 0.0f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass        = renderPass_.handle();
        rpInfo.framebuffer       = framebuffers_[imageIndex];
        rpInfo.renderArea.extent = swapchain_.swapchainExtent;
        rpInfo.clearValueCount   = 2;
        rpInfo.pClearValues      = clearValues;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        drawChunks(cmd, world);
        drawUI(cmd);

        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);
    }

    void drawUI(VkCommandBuffer& cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline_.pipeline);
        VkBuffer bufs[] = {uiPipeline_.vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, bufs, offsets);
        vkCmdDraw(cmd, uiPipeline_.vertexCount_, 1, 0, 0);
    }

    void drawChunks(VkCommandBuffer& cmd, World& world) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, chunkPipeline_.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    chunkPipeline_.pipelineLayout, 0, 1, &descriptorSet_, 0, nullptr);

        std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>& worldGrid = world.worldGrid;
        for (const auto& [key, value] : worldGrid) {
            glm::mat4 chunkModel = glm::translate(glm::mat4(1.0f), glm::vec3((float)key.x * 16.0f, 0.0f, (float)key.z * 16.0f));
            vkCmdPushConstants(
                cmd,
                chunkPipeline_.pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(glm::mat4),
                &chunkModel
            );
            uint32_t slotIndex = value.slot.slotOffset / sizeof(uint32_t);
            vkCmdDraw(cmd, static_cast<uint32_t>(value.faces.size() * 6), 1, 0, slotIndex);
        }

        vkCmdDraw(cmd, 6, 1, 0, 0);
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(ctx_.device, &semInfo, nullptr, &imageAvailable_) != VK_SUCCESS ||
            vkCreateSemaphore(ctx_.device, &semInfo, nullptr, &renderFinished_) != VK_SUCCESS ||
            vkCreateFence(ctx_.device, &fenceInfo, nullptr, &inFlight_) != VK_SUCCESS)
            throw std::runtime_error("failed to create sync objects");
    }

    // ─────────────────────────────────────────
    //  Resources
    // ─────────────────────────────────────────

    void createDepthResources() {
        createImage(swapchain_.swapchainExtent.width, swapchain_.swapchainExtent.height,
                    VK_FORMAT_D32_SFLOAT,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    depthImage_, depthMemory_);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image    = depthImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format   = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(ctx_.device, &viewInfo, nullptr, &depthImageView_);
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

    void createUniformBuffer() {
        VkDeviceSize size = sizeof(UniformBufferObject);
        createBuffer(size,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffer_, uniformMemory_);
        vkMapMemory(ctx_.device, uniformMemory_, 0, size, 0, &uniformMapped_);
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

    void createTextureImage() {
        int w, h, channels;
        stbi_uc* pixels = stbi_load("assets/textures/texture_atlas.png", &w, &h, &channels, STBI_rgb_alpha);
        if (!pixels) throw std::runtime_error("failed to load texture");

        VkDeviceSize imageSize = w * h * 4;

        VkBuffer       stagingBuffer;
        VkDeviceMemory stagingMemory;
        createBuffer(imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingMemory);

        void* data;
        vkMapMemory(ctx_.device, stagingMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, imageSize);
        vkUnmapMemory(ctx_.device, stagingMemory);
        stbi_image_free(pixels);

        createImage(w, h,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            textureImage_, textureMemory_);

        transitionImageLayout(textureImage_,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage_, w, h);
        transitionImageLayout(textureImage_,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(ctx_.device, stagingBuffer, nullptr);
        vkFreeMemory(ctx_.device, stagingMemory, nullptr);
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

    void createTextureImageView() {
        VkImageViewCreateInfo info{};
        info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image    = textureImage_;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format   = VK_FORMAT_R8G8B8A8_SRGB;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        vkCreateImageView(ctx_.device, &info, nullptr, &textureImageView_);
    }

    void createSampler() {
        VkSamplerCreateInfo info{};
        info.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter    = VK_FILTER_NEAREST;
        info.minFilter    = VK_FILTER_NEAREST;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        vkCreateSampler(ctx_.device, &info, nullptr, &textureSampler_);
    }
};