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

// Creates a graphics pipeline for textured geometry (position + UV).
// Uses textured.vert.spv and textured.frag.spv shaders.
// Descriptor set layout must have UBO at binding 0 and combined image sampler at binding 1.
bool createTexturedPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline);

// Creates a graphics pipeline for 3D model rendering.
// Vertex layout matches the full Vertex struct (position, normal, texcoord,
// tangent, bitangent, boneIDs, weights).
// Uses model.vert.spv and model.frag.spv shaders.
// Descriptor set layout: UBO at binding 0, diffuse sampler at binding 1.
bool createModelPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline);
