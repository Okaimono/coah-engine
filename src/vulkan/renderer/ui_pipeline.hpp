#pragma once
#include "vulkan_includes.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/swapchain.hpp"
#include "vulkan/renderer/render_pass.hpp"
#include "vulkan/command_manager.hpp"
#include "core/types.hpp"

#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <glm/glm.hpp>

class UiPipeline {
public:
    VkPipeline            pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout      pipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSet       = VK_NULL_HANDLE;

    void* mappedPtr_ = nullptr;
    uint32_t vertexCount_ = 0;
    VkBuffer       vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;

    uint32_t texW_ = 0, texH_ = 0;   // exposed publicly — game side can use these for manual UV math if needed

    UiPipeline(VulkanContext& ctx, Swapchain& swapchain, RenderPass& renderPass, CommandManager& commandManager)
        : ctx_(ctx)
        , swapchain_(swapchain)
        , renderPass_(renderPass)
        , commandManager_(commandManager)
    {
        createVertexBuffer();
        createAtlasTexture("assets/textures/items/ui_atlas.png");
        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSet();
        createPipeline();
    }

    ~UiPipeline() {
        if (mappedPtr_) {
            vkUnmapMemory(ctx_.device, vertexMemory);
            mappedPtr_ = nullptr;
        }
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(ctx_.device, pipeline, nullptr);
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(ctx_.device, pipelineLayout, nullptr);
        }
        if (vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx_.device, vertexBuffer, nullptr);
        }
        if (vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(ctx_.device, vertexMemory, nullptr);
        }
    }

    UiPipeline(const UiPipeline&)            = delete;
    UiPipeline& operator=(const UiPipeline&) = delete;
    UiPipeline(UiPipeline&&)                 = delete;
    UiPipeline& operator=(UiPipeline&&)      = delete;

    void updateVertexBuffer(const std::vector<UIVertex>& vertices) {
        assert(vertices.size() <= kMaxUiVertices && "UI vertex buffer overflow");

        VkDeviceSize size = vertices.size() * sizeof(UIVertex);
        memcpy(mappedPtr_, vertices.data(), size);
        vertexCount_ = (uint32_t)vertices.size();
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
    VulkanContext& ctx_;
    Swapchain&     swapchain_;
    RenderPass&    renderPass_;
    CommandManager& commandManager_;

    static constexpr VkDeviceSize kMaxUiVertices = 8192;
    static constexpr VkDeviceSize kUiVertexBufferSize = kMaxUiVertices * sizeof(UIVertex);

    VkImage        defaultTextureImage_  = VK_NULL_HANDLE;
    VkDeviceMemory defaultTextureMemory_ = VK_NULL_HANDLE;
    VkImageView    defaultTextureView_   = VK_NULL_HANDLE;
    VkSampler      defaultSampler_       = VK_NULL_HANDLE;

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

    void createVertexBuffer() {
        UIVertex positions[6] = {
            {{-0.25f, -0.25f}},
            {{ 0.25f, -0.25f}},
            {{ 0.25f,  0.25f}},
            {{-0.25f, -0.25f}},
            {{ 0.25f,  0.25f}},
            {{-0.25f,  0.25f}}
        };

        VkDeviceSize size = sizeof(positions);
        createBuffer(kUiVertexBufferSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertexBuffer, vertexMemory);
        vkMapMemory(ctx_.device, vertexMemory, 0, kUiVertexBufferSize, 0, &mappedPtr_);
        memcpy(mappedPtr_, positions, size);
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

    void createAtlasTexture(const std::string& path) {
        int texW, texH, texChannels;
        stbi_uc* pixels = stbi_load(path.c_str(), &texW, &texH, &texChannels, STBI_rgb_alpha);
        if (!pixels) {
            throw std::runtime_error("failed to load texture: " + path);
        }
        VkDeviceSize imageSize = texW * texH * 4;

        texW_ = (uint32_t)texW;
        texH_ = (uint32_t)texH;

        VkBuffer staging; VkDeviceMemory stagingMem;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    staging, stagingMem);

        void* data;
        vkMapMemory(ctx_.device, stagingMem, 0, imageSize, 0, &data);
        memcpy(data, pixels, (size_t)imageSize);
        vkUnmapMemory(ctx_.device, stagingMem);
        stbi_image_free(pixels);

        createImage(texW_, texH_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    defaultTextureImage_, defaultTextureMemory_);

        transitionImageLayout(defaultTextureImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(staging, defaultTextureImage_, texW_, texH_);
        transitionImageLayout(defaultTextureImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(ctx_.device, staging, nullptr);
        vkFreeMemory(ctx_.device, stagingMem, nullptr);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image    = defaultTextureImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format   = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(ctx_.device, &viewInfo, nullptr, &defaultTextureView_);

        VkSamplerCreateInfo sampInfo{};
        sampInfo.sType     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampInfo.magFilter = VK_FILTER_NEAREST;
        sampInfo.minFilter = VK_FILTER_NEAREST;
        sampInfo.addressModeU = sampInfo.addressModeV = sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(ctx_.device, &sampInfo, nullptr, &defaultSampler_);

        // No atlas_.init()/registerRegion() here anymore — UIContext owns the
        // name -> UVRect lookup table directly now, populated by hand with real
        // coordinates matching whatever's actually in this PNG.
    }

    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding         = 0;
        binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings    = &binding;
        vkCreateDescriptorSetLayout(ctx_.device, &info, nullptr, &descriptorSetLayout);
    }

    void createDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo info{};
        info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.poolSizeCount = 1;
        info.pPoolSizes    = &poolSize;
        info.maxSets       = 1;
        vkCreateDescriptorPool(ctx_.device, &info, nullptr, &descriptorPool);
    }

    void createDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &descriptorSetLayout;
        vkAllocateDescriptorSets(ctx_.device, &allocInfo, &descriptorSet);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = defaultTextureView_;
        imageInfo.sampler     = defaultSampler_;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = descriptorSet;
        write.dstBinding      = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo      = &imageInfo;
        vkUpdateDescriptorSets(ctx_.device, 1, &write, 0, nullptr);
    }

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

    void createPipeline() {
        auto vert = readFile("assets/shaders/ui.vert.spv");
        auto frag = readFile("assets/shaders/ui.frag.spv");
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

        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(UIVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrs[3]{};
        attrs[0].binding = 0; attrs[0].location = 0;
        attrs[0].format  = VK_FORMAT_R32G32_SFLOAT;
        attrs[0].offset  = offsetof(UIVertex, pos);

        attrs[1].binding = 0; attrs[1].location = 1;
        attrs[1].format  = VK_FORMAT_R32G32_SFLOAT;
        attrs[1].offset  = offsetof(UIVertex, uv);

        attrs[2].binding = 0; attrs[2].location = 2;
        attrs[2].format  = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[2].offset  = offsetof(UIVertex, color);

        VkPipelineVertexInputStateCreateInfo vertInput{};
        vertInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInput.vertexBindingDescriptionCount   = 1;
        vertInput.pVertexBindingDescriptions      = &binding;
        vertInput.vertexAttributeDescriptionCount = 3;
        vertInput.pVertexAttributeDescriptions    = attrs;

        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport{};
        viewport.width    = (float)swapchain_.swapchainExtent.width;
        viewport.height   = (float)swapchain_.swapchainExtent.height;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent = swapchain_.swapchainExtent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_NONE;
        raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable  = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState blendAttach{};
        blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttach.blendEnable         = VK_TRUE;
        blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blendAttach;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 1;
        layoutInfo.pSetLayouts            = &descriptorSetLayout;
        layoutInfo.pushConstantRangeCount = 0;
        layoutInfo.pPushConstantRanges    = nullptr;
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
        std::cout << "ui pipeline created\n";
    }
};