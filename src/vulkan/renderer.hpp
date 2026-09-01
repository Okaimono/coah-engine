#pragma once
#include "vulkan_context.hpp"
#include "swapchain.hpp"
#include "vulkan/renderer/render_pass.hpp"
#include "vulkan/renderer/descriptor_set_layout.hpp"
#include "vulkan/renderer/chunk_pipeline.hpp"
#include "vulkan/renderer/ui_pipeline.hpp"
#include "vulkan/renderer/entity_pipeline.hpp"
#include "vulkan/renderer/particle_pipeline.hpp"
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
        , commandManager_(ctx)   
        , allocator_(ctx, commandManager_)
        , descriptorSetLayout_(ctx)
        , chunkPipeline_(ctx, swapchain, renderPass_, descriptorSetLayout_.handle, allocator_, commandManager_)
        , uiPipeline_(ctx, swapchain, renderPass_, commandManager_)
        , entityPipeline_(ctx, swapchain, renderPass_, commandManager_)
        , particlePipeline_(ctx, swapchain, renderPass_, commandManager_)
    {
        createDepthResources();
        createFramebuffers();
        createCommandBuffers();
        createSyncObjects();
    }

    void updateEntityInstances(const std::vector<EntityInstance>& instances) {
        entityPipeline_.updateInstances(instances);
    }

    void updateParticleInstances(const std::vector<ParticleInstance>& instances) {
        particlePipeline_.updateInstances(instances);
    }

    Slot reserveChunkSlot(const std::vector<uint32_t>& faceData) {
        Slot slot = allocator_.reserveSlot();
        allocator_.updateSlot(slot, faceData);
        return slot;
    }

    void addRenderEntry(const ChunkRenderEntry& renderEntry) {
        chunkPipeline_.addRenderEntry(renderEntry);
    }

    void updateChunkSlot(Slot& slot, const std::vector<uint32_t>& faceData) {
        allocator_.updateSlot(slot, faceData);
    }

    void updateUI(const std::vector<UIVertex>& vertices) {
        uiPipeline_.updateVertexBuffer(vertices);
    }

    void updateUniformBuffer(const glm::mat4& view, const glm::mat4& proj) {
        chunkPipeline_.updateUniformBuffer(view, proj);
        entityPipeline_.updateCamera(view, proj);
        particlePipeline_.updateCamera(view, proj);
    }

    void drawFrame() {
        vkWaitForFences(ctx_.device, 1, &inFlight_, VK_TRUE, UINT64_MAX);
        vkResetFences(ctx_.device, 1, &inFlight_);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(ctx_.device, swapchain_.swapchain, UINT64_MAX, imageAvailable_, VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(commandBuffers_[imageIndex], 0);
        recordCommandBuffer(commandBuffers_[imageIndex], imageIndex);

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
        if (depthImageView_ != VK_NULL_HANDLE) vkDestroyImageView(ctx_.device, depthImageView_, nullptr);
        if (depthImage_     != VK_NULL_HANDLE) vkDestroyImage(ctx_.device, depthImage_, nullptr);
        if (depthMemory_    != VK_NULL_HANDLE) vkFreeMemory(ctx_.device, depthMemory_, nullptr);
        if (imageAvailable_ != VK_NULL_HANDLE) vkDestroySemaphore(ctx_.device, imageAvailable_, nullptr);
        if (renderFinished_ != VK_NULL_HANDLE) vkDestroySemaphore(ctx_.device, renderFinished_, nullptr);
        if (inFlight_       != VK_NULL_HANDLE) vkDestroyFence(ctx_.device, inFlight_, nullptr);
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
    EntityPipeline      entityPipeline_;
    ParticlePipeline     particlePipeline_;

    std::vector<VkFramebuffer>   framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;

    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;
    VkFence     inFlight_       = VK_NULL_HANDLE;

    VkImage        depthImage_     = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_    = VK_NULL_HANDLE;
    VkImageView    depthImageView_ = VK_NULL_HANDLE;

    uint32_t currentImageIndex_ = 0;

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

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
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

        chunkPipeline_.recordRenderEntries(cmd);
        entityPipeline_.recordEntities(cmd);
        uiPipeline_.recordUI(cmd);
        particlePipeline_.recordParticles(cmd);

        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);
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

    void createDepthResources() {
        VkImageCreateInfo info{};
        info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType     = VK_IMAGE_TYPE_2D;
        info.extent        = {swapchain_.swapchainExtent.width, swapchain_.swapchainExtent.height, 1};
        info.mipLevels     = 1;
        info.arrayLayers   = 1;
        info.format        = VK_FORMAT_D32_SFLOAT;
        info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        info.samples       = VK_SAMPLE_COUNT_1_BIT;
        info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(ctx_.device, &info, nullptr, &depthImage_) != VK_SUCCESS)
            throw std::runtime_error("failed to create depth image");

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(ctx_.device, depthImage_, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(ctx_.device, &allocInfo, nullptr, &depthMemory_) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate depth memory");
        vkBindImageMemory(ctx_.device, depthImage_, depthMemory_, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image    = depthImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format   = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(ctx_.device, &viewInfo, nullptr, &depthImageView_) != VK_SUCCESS)
            throw std::runtime_error("failed to create depth image view");
    }
};