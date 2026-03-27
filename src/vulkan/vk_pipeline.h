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
// Descriptor set layout: UBO at binding 0, diffuse sampler at binding 1, shadow sampler at binding 2.
bool createModelPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline);

// Creates a depth-only pipeline for shadow map rendering.
// Uses shadow.vert.spv and shadow.frag.spv shaders.
// Same vertex layout as model pipeline. No color attachments.
bool createShadowPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline);

// Creates a skybox pipeline.
// Vertex layout: vec3 position only. Depth test LEQUAL, no depth write.
// Push constant: mat4 viewProj. Descriptor set: samplerCube at binding 0.
bool createSkyboxPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline);

// Creates a pipeline for the ID buffer (mouse picking).
// Renders object IDs as R32_UINT. Same vertex layout as model pipeline.
// Push constant: mat4 model + uint objectID (68 bytes).
// Descriptor set: FrameUBO at binding 0, BoneUBO at binding 1.
bool createIDPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline);

// Creates a debug overlay pipeline that renders objects with false-color
// based on their object ID, with alpha blending on top of the main scene.
// Reuses id.vert, uses id_debug.frag. Renders to the main render pass.
bool createIDDebugPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline);
