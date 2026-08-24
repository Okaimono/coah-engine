#pragma once
#include "stb_image.h"
#include "vulkan_includes.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/swapchain.hpp"
#include "vulkan/renderer/render_pass.hpp"
#include "vulkan/pool_allocator.hpp"
#include "vulkan/command_manager.hpp"
#include "core/config.hpp"
#include "core/types.hpp"

#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <unordered_map>

struct ChunkRenderEntry {
    Slot slot;
    uint32_t faceSize;
    glm::mat4 chunkModel;
};

class ChunkPipeline {
public:
    VkPipeline       pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    ChunkPipeline(VulkanContext& ctx, Swapchain& swapchain, RenderPass& renderPass,
                  VkDescriptorSetLayout descriptorSetLayout, PoolAllocator& poolAllocator,
                  CommandManager& commandManager)
        : ctx_(ctx)
        , swapchain_(swapchain)
        , renderPass_(renderPass)
        , poolAllocator_(poolAllocator)
        , commandManager_(commandManager)
    {
        createPipeline(descriptorSetLayout);
        createUniformBuffer();
        createTextureImage();
        createTextureImageView();
        createSampler();
        createDescriptorPool();
        createDescriptorSet(descriptorSetLayout);
    }

    ~ChunkPipeline() {
        if (textureSampler_   != VK_NULL_HANDLE) vkDestroySampler(ctx_.device, textureSampler_, nullptr);
        if (textureImageView_ != VK_NULL_HANDLE) vkDestroyImageView(ctx_.device, textureImageView_, nullptr);
        if (textureImage_     != VK_NULL_HANDLE) vkDestroyImage(ctx_.device, textureImage_, nullptr);
        if (textureMemory_    != VK_NULL_HANDLE) vkFreeMemory(ctx_.device, textureMemory_, nullptr);
        if (uniformBuffer_    != VK_NULL_HANDLE) vkDestroyBuffer(ctx_.device, uniformBuffer_, nullptr);
        if (uniformMemory_    != VK_NULL_HANDLE) vkFreeMemory(ctx_.device, uniformMemory_, nullptr);
        if (descriptorPool_   != VK_NULL_HANDLE) vkDestroyDescriptorPool(ctx_.device, descriptorPool_, nullptr);
        if (pipeline           != VK_NULL_HANDLE) vkDestroyPipeline(ctx_.device, pipeline, nullptr);
        if (pipelineLayout     != VK_NULL_HANDLE) vkDestroyPipelineLayout(ctx_.device, pipelineLayout, nullptr);
    }

    ChunkPipeline(const ChunkPipeline&)            = delete;
    ChunkPipeline& operator=(const ChunkPipeline&) = delete;
    ChunkPipeline(ChunkPipeline&&)                 = delete;
    ChunkPipeline& operator=(ChunkPipeline&&)      = delete;

    void addRenderEntry(const ChunkRenderEntry& renderEntry) {
        renderEntries.push_back(renderEntry);
    }

    void updateUniformBuffer(const glm::mat4& view, const glm::mat4& proj) {
        UniformBufferObject ubo{};
        ubo.view = view;
        ubo.proj = proj;
        memcpy(uniformMapped_, &ubo, sizeof(ubo));
    }

    // No longer takes a descriptor set — it binds its own.
    void recordRenderEntries(VkCommandBuffer& cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, 0, 1, &descriptorSet_, 0, nullptr);

        for (const auto& entry : renderEntries) {
            vkCmdPushConstants(
                cmd,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(glm::mat4),
                &entry.chunkModel
            );
            uint32_t slotIndex = entry.slot.slotOffset / sizeof(uint32_t);
            vkCmdDraw(cmd, entry.faceSize, 1, 0, slotIndex);
        }
        renderEntries.clear();
    }

    static std::vector<char> readFile(const std::string& path) {
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) throw std::runtime_error("failed to open: " + path);
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        rewind(f);
        std::vector<char> buf(size);
        fread(buf.data(), 1, size, f);
        fclose(f);
        return buf;
    }

private:
    VulkanContext&  ctx_;
    Swapchain&      swapchain_;
    RenderPass&     renderPass_;
    PoolAllocator&  poolAllocator_;
    CommandManager& commandManager_;

    std::vector<ChunkRenderEntry> renderEntries;

    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet  descriptorSet_  = VK_NULL_HANDLE;

    VkBuffer       uniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniformMemory_ = VK_NULL_HANDLE;
    void*          uniformMapped_ = nullptr;

    VkImage        textureImage_     = VK_NULL_HANDLE;
    VkDeviceMemory textureMemory_    = VK_NULL_HANDLE;
    VkImageView    textureImageView_ = VK_NULL_HANDLE;
    VkSampler      textureSampler_   = VK_NULL_HANDLE;

    // ─────────────────────────────────────────
    //  Shader / pipeline creation
    // ─────────────────────────────────────────

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo info{};
        info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = code.size();
        info.pCode    = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(ctx_.device, &info, nullptr, &module) != VK_SUCCESS)
            throw std::runtime_error("failed to create shader module");
        return module;
    }

    void createPipeline(VkDescriptorSetLayout descriptorSetLayout) {
        auto vert = readFile("assets/shaders/chunk.vert.spv");
        auto frag = readFile("assets/shaders/chunk.frag.spv");
        VkShaderModule vertMod = createShaderModule(vert);
        VkShaderModule fragMod = createShaderModule(frag);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertMod;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragMod;
        stages[1].pName  = "main";

        VkPipelineVertexInputStateCreateInfo vertInput{};
        vertInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInput.vertexBindingDescriptionCount   = 0;
        vertInput.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = (float)Config::GAME_WIDTH;
        viewport.height   = (float)Config::GAME_HEIGHT;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = { (uint32_t)Config::GAME_WIDTH, (uint32_t)Config::GAME_HEIGHT };

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_BACK_BIT;
        raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable  = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState blendAttach{};
        blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blendAttach;

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset     = 0;
        pushRange.size       = sizeof(glm::mat4);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount          = 1;
        layoutInfo.pSetLayouts             = &descriptorSetLayout;
        layoutInfo.pushConstantRangeCount  = 1;
        layoutInfo.pPushConstantRanges     = &pushRange;
        if (vkCreatePipelineLayout(ctx_.device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create pipeline layout");

        VkGraphicsPipelineCreateInfo info{};
        info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.stageCount          = 2;
        info.pStages             = stages;
        info.pVertexInputState   = &vertInput;
        info.pInputAssemblyState = &assembly;
        info.pViewportState      = &viewportState;
        info.pRasterizationState = &raster;
        info.pDepthStencilState  = &depthStencil;
        info.pMultisampleState   = &ms;
        info.pColorBlendState    = &blend;
        info.layout              = pipelineLayout;
        info.renderPass          = renderPass_.handle();

        if (vkCreateGraphicsPipelines(ctx_.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
            throw std::runtime_error("failed to create pipeline");

        vkDestroyShaderModule(ctx_.device, vertMod, nullptr);
        vkDestroyShaderModule(ctx_.device, fragMod, nullptr);
        std::cout << "chunk pipeline created\n";
    }

    // ─────────────────────────────────────────
    //  Resources (moved from Renderer — duplicated on purpose)
    // ─────────────────────────────────────────

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo info{};
        info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size        = size;
        info.usage       = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(ctx_.device, &info, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create buffer");

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(ctx_.device, buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits, properties);
        if (vkAllocateMemory(ctx_.device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate buffer memory");
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
        if (vkCreateImage(ctx_.device, &info, nullptr, &image) != VK_SUCCESS)
            throw std::runtime_error("failed to create image");

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(ctx_.device, image, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits, properties);
        if (vkAllocateMemory(ctx_.device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory");
        vkBindImageMemory(ctx_.device, image, memory, 0);
    }

    void createUniformBuffer() {
        VkDeviceSize size = sizeof(UniformBufferObject);
        createBuffer(size,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffer_, uniformMemory_);
        vkMapMemory(ctx_.device, uniformMemory_, 0, size, 0, &uniformMapped_);
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
        if (vkCreateImageView(ctx_.device, &info, nullptr, &textureImageView_) != VK_SUCCESS)
            throw std::runtime_error("failed to create texture image view");
    }

    void createSampler() {
        VkSamplerCreateInfo info{};
        info.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter    = VK_FILTER_NEAREST;
        info.minFilter    = VK_FILTER_NEAREST;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        if (vkCreateSampler(ctx_.device, &info, nullptr, &textureSampler_) != VK_SUCCESS)
            throw std::runtime_error("failed to create sampler");
    }

    // ─────────────────────────────────────────
    //  Descriptors
    // ─────────────────────────────────────────

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
        if (vkCreateDescriptorPool(ctx_.device, &info, nullptr, &descriptorPool_) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool");
    }

    void createDescriptorSet(VkDescriptorSetLayout descriptorSetLayout) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = descriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &descriptorSetLayout;
        if (vkAllocateDescriptorSets(ctx_.device, &allocInfo, &descriptorSet_) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor set");

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = uniformBuffer_;
        uboInfo.offset = 0;
        uboInfo.range  = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo ssboInfo{};
        ssboInfo.buffer = poolAllocator_.ssboBuffer;
        ssboInfo.offset = 0;
        ssboInfo.range  = poolAllocator_.poolSize;

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
};