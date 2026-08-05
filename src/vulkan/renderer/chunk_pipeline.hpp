#pragma once
#include "vulkan_includes.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/swapchain.hpp"
#include "vulkan/renderer/render_pass.hpp"
#include "core/config.hpp"

#include <vector>
#include <string>
#include <cstdio>
#include <stdexcept>
#include <iostream>

class ChunkPipeline {
public:
    VkPipeline       pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    ChunkPipeline(VulkanContext& ctx, Swapchain& swapchain, RenderPass& renderPass,
                  VkDescriptorSetLayout descriptorSetLayout)
        : ctx_(ctx)
        , swapchain_(swapchain)
        , renderPass_(renderPass)
    {
        createPipeline(descriptorSetLayout);
    }

    ~ChunkPipeline() {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(ctx_.device, pipeline, nullptr);
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(ctx_.device, pipelineLayout, nullptr);
        }
    }

    ChunkPipeline(const ChunkPipeline&)            = delete;
    ChunkPipeline& operator=(const ChunkPipeline&) = delete;
    ChunkPipeline(ChunkPipeline&&)                 = delete;
    ChunkPipeline& operator=(ChunkPipeline&&)      = delete;

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

        // No vertex buffers — all geometry comes from SSBO
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
        std::cout << "pipeline created\n";
    }
};