#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

// Loads a SPIR-V shader module from disk
VkShaderModule createShaderModule(VkDevice device, const std::string& filepath);

// Creates the graphics pipeline for our triangle.
// Returns false on failure, fills outPipeline and outPipelineLayout.
bool createTrianglePipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline);
