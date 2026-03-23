#include "vk_pipeline.h"
#include <fstream>
#include <iostream>
#include <array>
#include <glm/glm.hpp>

// ─── Shader Module ───────────────────────────────────────────────────────────
//
// SPIR-V is the intermediate binary format for Vulkan shaders (like bytecode).
// We compile GLSL 450 → SPIR-V at build time with glslc, then load the .spv
// file here and wrap it in a VkShaderModule.

static std::vector<char> readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[Vulkan] Failed to open shader file: " << filepath << std::endl;
        return {};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::string& filepath) {
    auto code = readFile(filepath);
    if (code.empty()) return VK_NULL_HANDLE;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create shader module from: " << filepath << std::endl;
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

// ─── Graphics Pipeline ───────────────────────────────────────────────────────
//
// In OpenGL, pipeline state is set with individual calls (glEnable, glBlendFunc,
// glDepthFunc...) and can change at any time. In Vulkan, ALL pipeline state is
// baked into a single immutable VkPipeline object at creation time:
//
//   - Shader stages (which .spv files to run)
//   - Vertex input (attribute layout — positions, colors, etc.)
//   - Input assembly (triangle list, strip, etc.)
//   - Viewport/scissor (can be dynamic so we don't need to recreate on resize)
//   - Rasterizer (fill mode, cull mode, front face)
//   - Multisampling
//   - Depth/stencil test
//   - Color blending
//   - Pipeline layout (descriptor set layouts, push constants)
//
// This upfront compilation lets the driver optimize aggressively. The tradeoff
// is you need separate pipeline objects for different render configurations.

// Vertex layout for our triangle — matches the GLSL shader inputs
struct TriangleVertex {
    glm::vec3 position; // layout(location = 0)
    glm::vec3 color;    // layout(location = 1)

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(TriangleVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attrs{};

        // Position at location 0
        attrs[0].binding  = 0;
        attrs[0].location = 0;
        attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset   = offsetof(TriangleVertex, position);

        // Color at location 1
        attrs[1].binding  = 0;
        attrs[1].location = 1;
        attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[1].offset   = offsetof(TriangleVertex, color);

        return attrs;
    }
};

bool createTrianglePipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline)
{
    // ── Shader stages ────────────────────────────────────────────────────────
    VkShaderModule vertModule = createShaderModule(device, "shaders/vulkan/compiled/triangle.vert.spv");
    VkShaderModule fragModule = createShaderModule(device, "shaders/vulkan/compiled/triangle.frag.spv");

    if (!vertModule || !fragModule) {
        std::cerr << "[Vulkan] Failed to load triangle shaders" << std::endl;
        return false;
    }

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName  = "main"; // Entry point function name

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};

    // ── Vertex input ─────────────────────────────────────────────────────────
    auto bindingDesc = TriangleVertex::getBindingDescription();
    auto attrDescs   = TriangleVertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount    = 1;
    vertexInput.pVertexBindingDescriptions       = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount  = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions     = attrDescs.data();

    // ── Input assembly ───────────────────────────────────────────────────────
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ── Dynamic state (viewport + scissor) ───────────────────────────────────
    // Making these dynamic means we don't need to recreate the pipeline on resize
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // ── Rasterizer ───────────────────────────────────────────────────────────
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    // ── Multisampling (disabled for now) ─────────────────────────────────────
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ── Depth/stencil ────────────────────────────────────────────────────────
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    // ── Color blending (no blending, just write) ─────────────────────────────
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    // ── Pipeline layout (descriptor sets) ────────────────────────────────────
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &outPipelineLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create pipeline layout" << std::endl;
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    // ── Create the pipeline ──────────────────────────────────────────────────
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = outPipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                   nullptr, &outPipeline) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create graphics pipeline" << std::endl;
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    // Shader modules can be destroyed after pipeline creation
    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    std::cout << "[Vulkan] Triangle pipeline created" << std::endl;
    return true;
}

// ─── Textured Pipeline ──────────────────────────────────────────────────────
//
// Same structure as the triangle pipeline, but with a different vertex layout:
//   - position (vec3) at location 0
//   - texCoord (vec2) at location 1
//
// The descriptor set layout includes a combined image sampler at binding 1
// in addition to the UBO at binding 0.

struct TexturedVertex {
    glm::vec3 position; // layout(location = 0)
    glm::vec2 texCoord; // layout(location = 1)

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(TexturedVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attrs{};

        // Position at location 0
        attrs[0].binding  = 0;
        attrs[0].location = 0;
        attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset   = offsetof(TexturedVertex, position);

        // UV at location 1
        attrs[1].binding  = 0;
        attrs[1].location = 1;
        attrs[1].format   = VK_FORMAT_R32G32_SFLOAT;
        attrs[1].offset   = offsetof(TexturedVertex, texCoord);

        return attrs;
    }
};

bool createTexturedPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline)
{
    // ── Shader stages ────────────────────────────────────────────────────────
    VkShaderModule vertModule = createShaderModule(device, "shaders/vulkan/compiled/textured.vert.spv");
    VkShaderModule fragModule = createShaderModule(device, "shaders/vulkan/compiled/textured.frag.spv");

    if (!vertModule || !fragModule) {
        std::cerr << "[Vulkan] Failed to load textured shaders" << std::endl;
        if (vertModule) vkDestroyShaderModule(device, vertModule, nullptr);
        if (fragModule) vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};

    // ── Vertex input (position + UV) ────────────────────────────────────────
    auto bindingDesc = TexturedVertex::getBindingDescription();
    auto attrDescs   = TexturedVertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount    = 1;
    vertexInput.pVertexBindingDescriptions       = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount  = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions     = attrDescs.data();

    // ── Input assembly ──────────────────────────────────────────────────────
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ── Dynamic state ───────────────────────────────────────────────────────
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // ── Rasterizer ──────────────────────────────────────────────────────────
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    // ── Multisampling ───────────────────────────────────────────────────────
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ── Depth/stencil ───────────────────────────────────────────────────────
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    // ── Color blending ──────────────────────────────────────────────────────
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    // ── Pipeline layout ─────────────────────────────────────────────────────
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &outPipelineLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create textured pipeline layout" << std::endl;
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    // ── Create pipeline ─────────────────────────────────────────────────────
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = outPipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                   nullptr, &outPipeline) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create textured pipeline" << std::endl;
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    std::cout << "[Vulkan] Textured pipeline created" << std::endl;
    return true;
}

// ─── Model Pipeline ─────────────────────────────────────────────────────────
//
// Pipeline for rendering 3D models loaded via Assimp. Uses the full Vertex
// struct layout with 7 attributes. This is the Vulkan equivalent of the
// vertex attribute setup in GLMeshData::setup().
//
// The key difference from the textured quad pipeline is the vertex layout —
// the Vertex struct is 152 bytes with position, normal, texcoords, tangent,
// bitangent, bone IDs, and bone weights.

#include "../vertex.h"

bool createModelPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout descriptorSetLayout,
    VkPipelineLayout& outPipelineLayout,
    VkPipeline& outPipeline)
{
    // ── Shader stages ────────────────────────────────────────────────────────
    VkShaderModule vertModule = createShaderModule(device, "shaders/vulkan/compiled/model.vert.spv");
    VkShaderModule fragModule = createShaderModule(device, "shaders/vulkan/compiled/model.frag.spv");

    if (!vertModule || !fragModule) {
        std::cerr << "[Vulkan] Failed to load model shaders" << std::endl;
        if (vertModule) vkDestroyShaderModule(device, vertModule, nullptr);
        if (fragModule) vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};

    // ── Vertex input — full Vertex struct (7 attributes) ────────────────────
    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 7> attrs{};

    // 0: Position (vec3)
    attrs[0].binding  = 0;
    attrs[0].location = 0;
    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset   = offsetof(Vertex, Position);

    // 1: Normal (vec3)
    attrs[1].binding  = 0;
    attrs[1].location = 1;
    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset   = offsetof(Vertex, Normal);

    // 2: TexCoords (vec2)
    attrs[2].binding  = 0;
    attrs[2].location = 2;
    attrs[2].format   = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset   = offsetof(Vertex, TexCoords);

    // 3: Tangent (vec3)
    attrs[3].binding  = 0;
    attrs[3].location = 3;
    attrs[3].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[3].offset   = offsetof(Vertex, Tangent);

    // 4: Bitangent (vec3)
    attrs[4].binding  = 0;
    attrs[4].location = 4;
    attrs[4].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[4].offset   = offsetof(Vertex, Bitangent);

    // 5: BoneIDs (ivec4) — integer format
    attrs[5].binding  = 0;
    attrs[5].location = 5;
    attrs[5].format   = VK_FORMAT_R32G32B32A32_SINT;
    attrs[5].offset   = offsetof(Vertex, m_BoneIDs);

    // 6: Weights (vec4)
    attrs[6].binding  = 0;
    attrs[6].location = 6;
    attrs[6].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[6].offset   = offsetof(Vertex, m_Weights);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount    = 1;
    vertexInput.pVertexBindingDescriptions       = &binding;
    vertexInput.vertexAttributeDescriptionCount  = static_cast<uint32_t>(attrs.size());
    vertexInput.pVertexAttributeDescriptions     = attrs.data();

    // ── Remaining pipeline state (same as textured pipeline) ────────────────
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &outPipelineLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create model pipeline layout" << std::endl;
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = outPipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                   nullptr, &outPipeline) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create model pipeline" << std::endl;
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    std::cout << "[Vulkan] Model pipeline created" << std::endl;
    return true;
}
