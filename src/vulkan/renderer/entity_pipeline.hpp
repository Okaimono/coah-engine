#pragma once
#include <glm/gtc/quaternion.hpp>

#include "stb_image.h"
#include "vulkan_includes.hpp"
#include "vulkan/vulkan_context.hpp"
#include "vulkan/swapchain.hpp"
#include "vulkan/renderer/render_pass.hpp"
#include "vulkan/command_manager.hpp"
#include "core/config.hpp"
#include "core/types.hpp"

#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <cassert>
#include <unordered_map>

// Per-vertex data — 8 unique positions x however many verts per cube (36, unindexed).
struct EntityVertex {
    glm::vec3 pos;
    glm::vec2 uv;
};

enum class Entity : uint32_t {
    NONE = 0,
    MORNING_STAR_ARROW,
    YELLOW
};

// Per-instance data — one of these per entity drawn this frame.
struct EntityInstance {
    glm::vec3 worldPos;
    float     size;
    glm::vec4 rotation = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
};

std::unordered_map<Entity, UVRect> entityAtlasRegions_ = {
    {Entity::NONE, UVRect{0.0f, 0.0f, 0.0078f, 0.0078f}},
    {Entity::YELLOW, UVRect{0.0625f, 0.96875f, 0.09375f, 1.0f}},
};

struct MeshRange {
    uint32_t firstVertex;
    uint32_t vertexCount;
};

std::unordered_map<Entity, MeshRange> meshTable_;

class EntityPipeline {
public:
    VkPipeline            pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout      pipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSet       = VK_NULL_HANDLE;

    VkBuffer       instanceBuffer     = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory     = VK_NULL_HANDLE;
    void*          instanceMappedPtr_ = nullptr;
    uint32_t       instanceCount_     = 0;

    VkBuffer       cubeVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory cubeVertexMemory = VK_NULL_HANDLE;

    EntityPipeline(VulkanContext& ctx, Swapchain& swapchain, RenderPass& renderPass, CommandManager& commandManager)
        : ctx_(ctx)
        , swapchain_(swapchain)
        , renderPass_(renderPass)
        , commandManager_(commandManager)
    {
        createCubeVertexBuffer();
        createInstanceBuffer();
        createTestInstance();
        createTextureImage();
        createTextureImageView();
        createSampler();
        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSet();
        createPipeline(descriptorSetLayout);
    }

    ~EntityPipeline() {
        if (instanceMappedPtr_) vkUnmapMemory(ctx_.device, instanceMemory);

        if (textureSampler_    != VK_NULL_HANDLE) vkDestroySampler(ctx_.device, textureSampler_, nullptr);
        if (textureView_       != VK_NULL_HANDLE) vkDestroyImageView(ctx_.device, textureView_, nullptr);
        if (textureImage_      != VK_NULL_HANDLE) vkDestroyImage(ctx_.device, textureImage_, nullptr);
        if (textureMemory_     != VK_NULL_HANDLE) vkFreeMemory(ctx_.device, textureMemory_, nullptr);
        if (descriptorPool     != VK_NULL_HANDLE) vkDestroyDescriptorPool(ctx_.device, descriptorPool, nullptr);
        if (descriptorSetLayout!= VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(ctx_.device, descriptorSetLayout, nullptr);
        if (pipeline            != VK_NULL_HANDLE) vkDestroyPipeline(ctx_.device, pipeline, nullptr);
        if (pipelineLayout      != VK_NULL_HANDLE) vkDestroyPipelineLayout(ctx_.device, pipelineLayout, nullptr);
        if (instanceBuffer      != VK_NULL_HANDLE) vkDestroyBuffer(ctx_.device, instanceBuffer, nullptr);
        if (instanceMemory      != VK_NULL_HANDLE) vkFreeMemory(ctx_.device, instanceMemory, nullptr);
        if (cubeVertexBuffer    != VK_NULL_HANDLE) vkDestroyBuffer(ctx_.device, cubeVertexBuffer, nullptr);
        if (cubeVertexMemory    != VK_NULL_HANDLE) vkFreeMemory(ctx_.device, cubeVertexMemory, nullptr);
    }

    EntityPipeline(const EntityPipeline&)            = delete;
    EntityPipeline& operator=(const EntityPipeline&) = delete;
    EntityPipeline(EntityPipeline&&)                 = delete;
    EntityPipeline& operator=(EntityPipeline&&)      = delete;

    // Called once per frame with the full list of entities to draw this frame.
    void updateInstances(const std::vector<EntityInstance>& instances) {
        assert(instances.size() <= kMaxInstances && "EntityPipeline instance overflow");

        VkDeviceSize size = instances.size() * sizeof(EntityInstance);
        memcpy(instanceMappedPtr_, instances.data(), size);
        instanceCount_ = (uint32_t)instances.size();
    }

    glm::mat4 view_ = glm::mat4(1.0f);
    glm::mat4 proj_ = glm::mat4(1.0f);

    void updateCamera(const glm::mat4& view, const glm::mat4& proj) {
        view_ = view;
        proj_ = proj;
    }

    void createTestInstance() {
        EntityInstance test{};
        test.worldPos = glm::vec3(0.0f, 50.0f, 0.0f);
        test.size     = 1.0f;

        glm::quat q = glm::angleAxis(glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        test.rotation = glm::vec4(q.x, q.y, q.z, q.w);

        memcpy(instanceMappedPtr_, &test, sizeof(test));
        instanceCount_ = 1;
    }
    
    // Records the draw for this frame's instances. Call after updateInstances().
    void recordEntities(VkCommandBuffer cmd) {
        if (instanceCount_ == 0) return;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        VkBuffer bufs[]        = { cubeVertexBuffer, instanceBuffer };
        VkDeviceSize offsets[] = { 0, 0 };
        vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);

        struct { glm::mat4 view; glm::mat4 proj; } pushData{ view_, proj_ };

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                            0, sizeof(pushData), &pushData);

        vkCmdDraw(cmd, 36, instanceCount_, 0, 0);
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
    CommandManager& commandManager_;

    VkImage        textureImage_   = VK_NULL_HANDLE;
    VkDeviceMemory textureMemory_  = VK_NULL_HANDLE;
    VkImageView    textureView_    = VK_NULL_HANDLE;
    VkSampler      textureSampler_ = VK_NULL_HANDLE;

    static constexpr uint32_t kMaxInstances = 4096;

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
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
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
        vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        commandManager_.endOneShot(cmd);
    }

    void createTextureImage() {
        int w, h, channels;
        stbi_uc* pixels = stbi_load("assets/textures/items/entity_atlas.png", &w, &h, &channels, STBI_rgb_alpha);
        if (!pixels) throw std::runtime_error("failed to load entity texture");

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

        transitionImageLayout(textureImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage_, w, h);
        transitionImageLayout(textureImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(ctx_.device, stagingBuffer, nullptr);
        vkFreeMemory(ctx_.device, stagingMemory, nullptr);
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
        if (vkCreateImageView(ctx_.device, &info, nullptr, &textureView_) != VK_SUCCESS)
            throw std::runtime_error("failed to create entity texture image view");
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
            throw std::runtime_error("failed to create entity sampler");
    }

    void createWhiteTexture() {
        uint32_t whitePixel = 0xFFFFFFFF; // RGBA all 1.0 in unorm/srgb 8-bit
        VkDeviceSize imageSize = 4;

        VkBuffer       stagingBuffer;
        VkDeviceMemory stagingMemory;
        createBuffer(imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingMemory);

        void* data;
        vkMapMemory(ctx_.device, stagingMemory, 0, imageSize, 0, &data);
        memcpy(data, &whitePixel, imageSize);
        vkUnmapMemory(ctx_.device, stagingMemory);

        createImage(1, 1,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            textureImage_, textureMemory_);

        transitionImageLayout(textureImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage_, 1, 1);
        transitionImageLayout(textureImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(ctx_.device, stagingBuffer, nullptr);
        vkFreeMemory(ctx_.device, stagingMemory, nullptr);
    }

    // 36 unindexed verts (6 faces x 2 tris x 3 verts). Every face samples the
    // full [0,0]-[1,1] range of the same texture, since all six sides match.

    // void createEntityMeshBuffer() {
    //     std::vector<EntityVertex> allVerts;

    //     auto appendMesh = [&](const Entity entity, const std::vector<EntityVertex>& verts) {
    //         MeshRange range;
    //         range.firstVertex = (uint32_t)allVerts.size();
    //         range.vertexCount  = (uint32_t)verts.size();
    //         meshTable_[entity] = range;
    //         allVerts.insert(allVerts.end(), verts.begin(), verts.end())
    //     };
    // }

    void createCubeVertexBuffer() {
        glm::vec3 p[8] = {
            {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, // back  (z-)
            {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, // front (z+)
        };

        struct Face { int a, b, c, d; };
        static const Face faces[6] = {
            {1,5,6,2}, // +X
            {4,0,3,7}, // -X
            {3,2,6,7}, // +Y
            {4,5,1,0}, // -Y
            {5,4,7,6}, // +Z
            {0,1,2,3}, // -Z
        };

        auto it = entityAtlasRegions_.find(Entity::YELLOW);
        UVRect uvCoords = it->second;
        
        glm::vec2 uv00{uvCoords.u0, uvCoords.v0}, 
                uv10{uvCoords.u1, uvCoords.v0}, 
                uv11{uvCoords.u1, uvCoords.v1}, 
                uv01{uvCoords.u0, uvCoords.v1};

        std::vector<EntityVertex> verts;
        verts.reserve(36);
        for (const Face& f : faces) {
            verts.push_back({p[f.a], uv00});
            verts.push_back({p[f.b], uv10});
            verts.push_back({p[f.c], uv11});
            verts.push_back({p[f.a], uv00});
            verts.push_back({p[f.c], uv11});
            verts.push_back({p[f.d], uv01});
        }

        VkDeviceSize size = verts.size() * sizeof(EntityVertex);
        createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     cubeVertexBuffer, cubeVertexMemory);
        void* data;
        vkMapMemory(ctx_.device, cubeVertexMemory, 0, size, 0, &data);
        memcpy(data, verts.data(), size);
        vkUnmapMemory(ctx_.device, cubeVertexMemory);
    }

    void createInstanceBuffer() {
        VkDeviceSize size = kMaxInstances * sizeof(EntityInstance);
        createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     instanceBuffer, instanceMemory);
        vkMapMemory(ctx_.device, instanceMemory, 0, size, 0, &instanceMappedPtr_);
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

    void createPipeline(VkDescriptorSetLayout setLayout) {
        auto vert = readFile("assets/shaders/entity.vert.spv");
        auto frag = readFile("assets/shaders/entity.frag.spv");
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

        VkVertexInputBindingDescription bindings[2]{};
        bindings[0].binding   = 0;
        bindings[0].stride    = sizeof(EntityVertex);
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        bindings[1].binding   = 1;
        bindings[1].stride    = sizeof(EntityInstance);
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        VkVertexInputAttributeDescription attrs[5]{};
        attrs[0].binding = 0; attrs[0].location = 0;          // vertex position
        attrs[0].format  = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset  = offsetof(EntityVertex, pos);

        attrs[1].binding = 0; attrs[1].location = 1;          // vertex uv
        attrs[1].format  = VK_FORMAT_R32G32_SFLOAT;
        attrs[1].offset  = offsetof(EntityVertex, uv);

        attrs[2].binding = 1; attrs[2].location = 2;          // instance worldPos
        attrs[2].format  = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[2].offset  = offsetof(EntityInstance, worldPos);

        attrs[3].binding = 1; attrs[3].location = 3;          // instance size
        attrs[3].format  = VK_FORMAT_R32_SFLOAT;
        attrs[3].offset  = offsetof(EntityInstance, size);

        attrs[4].binding = 1; attrs[4].location = 4;          // instance tint
        attrs[4].format  = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[4].offset  = offsetof(EntityInstance, rotation);

        VkPipelineVertexInputStateCreateInfo vertInput{};
        vertInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInput.vertexBindingDescriptionCount   = 2;
        vertInput.pVertexBindingDescriptions      = bindings;
        vertInput.vertexAttributeDescriptionCount = 5;
        vertInput.pVertexAttributeDescriptions    = attrs;

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
        raster.cullMode    = VK_CULL_MODE_BACK_BIT;   // solid mesh w/ consistent winding now
        raster.frontFace   = VK_FRONT_FACE_CLOCKWISE;
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
        blendAttach.blendEnable         = VK_TRUE;
        blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blendAttach;

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset     = 0;
        pushRange.size       = sizeof(glm::mat4) * 2;   // view + proj

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 1;
        layoutInfo.pSetLayouts            = &setLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pushRange;
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
        std::cout << "entity pipeline created\n";
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
        if (vkCreateDescriptorSetLayout(ctx_.device, &info, nullptr, &descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create entity descriptor set layout");
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
        if (vkCreateDescriptorPool(ctx_.device, &info, nullptr, &descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create entity descriptor pool");
    }

    void createDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &descriptorSetLayout;
        if (vkAllocateDescriptorSets(ctx_.device, &allocInfo, &descriptorSet) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate entity descriptor set");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = textureView_;
        imageInfo.sampler     = textureSampler_;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = descriptorSet;
        write.dstBinding      = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo      = &imageInfo;
        vkUpdateDescriptorSets(ctx_.device, 1, &write, 0, nullptr);
    }
};