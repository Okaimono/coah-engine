#pragma once
#include "vulkan_includes.hpp"
#include "vulkan/vulkan_context.hpp"

#include <vector>

class DescriptorSetLayout {
public:
    VkDescriptorSetLayout handle = VK_NULL_HANDLE;

    DescriptorSetLayout(VulkanContext& ctx)
        : ctx_(ctx)
    {
        create();
    }

    ~DescriptorSetLayout() {
        if (handle != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(ctx_.device, handle, nullptr);
        }
    }

    DescriptorSetLayout(const DescriptorSetLayout&)            = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout(DescriptorSetLayout&&)                 = delete;
    DescriptorSetLayout& operator=(DescriptorSetLayout&&)      = delete;

private:
    VulkanContext& ctx_;

    void create() {
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding         = 0;
        uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding ssboBinding{};
        ssboBinding.binding         = 1;
        ssboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBinding.descriptorCount = 1;
        ssboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding         = 2;
        samplerBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding bindings[] = {uboBinding, ssboBinding, samplerBinding};

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 3;
        info.pBindings    = bindings;

        if (vkCreateDescriptorSetLayout(ctx_.device, &info, nullptr, &handle) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor set layout");
    }
};