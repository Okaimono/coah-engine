#pragma once
#include "vulkan_includes.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/swapchain.hpp"
#include "vulkan/renderer/render_pass.hpp"
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
    VkPipeline       pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    void* mappedPtr_ = nullptr;
    uint32_t vertexCount_ = 0;
    VkBuffer       vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;

    UiPipeline(VulkanContext& ctx, Swapchain& swapchain, RenderPass& renderPass)
        : ctx_(ctx)
        , swapchain_(swapchain)
        , renderPass_(renderPass)
    {
        createVertexBuffer();
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

    static constexpr VkDeviceSize kMaxUiVertices = 8192;
    static constexpr VkDeviceSize kUiVertexBufferSize = kMaxUiVertices * sizeof(UIVertex);

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

        VkVertexInputAttributeDescription attr{};
        attr.binding  = 0;
        attr.location = 0;
        attr.format   = VK_FORMAT_R32G32_SFLOAT;
        attr.offset   = 0;

        VkPipelineVertexInputStateCreateInfo vertInput{};
        vertInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInput.vertexBindingDescriptionCount   = 1;
        vertInput.pVertexBindingDescriptions      = &binding;
        vertInput.vertexAttributeDescriptionCount = 1;
        vertInput.pVertexAttributeDescriptions    = &attr;

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
        layoutInfo.setLayoutCount         = 0;
        layoutInfo.pSetLayouts            = nullptr;
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