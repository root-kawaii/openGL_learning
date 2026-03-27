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
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

// Creates a graphics pipeline for textured geometry (position + UV).
// Uses textured.vert.spv and textured.frag.spv shaders.
// Descriptor set layout must have UBO at binding 0 and combined image sampler at binding 1.
bool createTexturedPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

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
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

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
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

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
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

// Creates a fullscreen post-processing quad pipeline.
// No vertex input. Renders a fullscreen triangle using gl_VertexIndex.
// Descriptor set: combined image sampler at binding 0.
bool createQuadPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

// Creates an infinite grid pipeline.
// No vertex input. Fragment shader reconstructs world position via ray-plane intersection.
// Descriptor set: FrameUBO at binding 0.
// Push constant: vec4 cameraPos (16 bytes).
bool createGridPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

// Creates a PBR pipeline (Cook-Torrance BRDF + normal map + shadow).
// Same vertex layout as model pipeline. Push constant: PBRPushConstant (80 bytes).
// Descriptor layout: FrameUBO (0), albedo (1), normal (2), metallic (3),
//                    roughness (4), shadow (5), BoneUBO (6).
bool createPBRPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

// Creates the instanced grass pipeline (no geometry shader — CPU pre-expanded blades).
// Binding 0 (per-vertex, VERTEX rate):   vec3 pos, vec3 normal, vec2 uv
// Binding 1 (per-instance, INSTANCE rate): vec3 pos, float rotation, float scale,
//                                           float heightVar, vec3 tint
// Push constant: GrassPushConstant (16 bytes: time, windStrength, windSpeed, grassHeight).
// Descriptor layout: FrameUBO (0), grass texture (1).
// Alpha blending + depth test (no depth write for transparent geometry).
bool createGrassPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

// Creates the water surface pipeline.
// Vertex layout: vec3 position (binding 0, location 0) + vec2 uv (location 1).
// Push constant: WaterPushConstant (16 bytes: time, waveHeight, waveSpeed, waterLevel).
// Descriptor layout: FrameUBO (0), normalMap (1), reflectionTex (2).
// Alpha blending enabled (water is semi-transparent).
bool createWaterPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

// Creates a UI text pipeline for glyph atlas rendering.
// Vertex layout: vec2 pos + vec2 uv.
// Push constant: mat4 orthoProj + vec4 color (80 bytes).
// Descriptor set: R8_UNORM glyph atlas at binding 0.
// Alpha blending enabled, no depth test/write.
bool createUITextPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

// Creates a UI sprite pipeline for RGBA texture rendering.
// Same vertex layout and push constant as text pipeline.
// Descriptor set: RGBA sampler2D at binding 0.
// Alpha blending enabled, no depth test/write.
bool createUISpritePipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline,
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
