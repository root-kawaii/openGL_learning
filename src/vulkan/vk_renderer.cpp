#include <glad/glad.h>   // Must be before GLFW (pulled in by vk_context.h)
#include "vk_renderer.h"
#include "vk_pipeline.h"
#include "vk_texture.h"
#include "../model.h"
#include "../scene.h"
#include <assimp/scene.h>    // aiTexture — for embedded GLB texture access
#include <ft2build.h>
#include FT_FREETYPE_H
#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <ImGuizmo/ImGuizmo.h>
#include <iostream>
#include <limits>
#include <cstring>
#include <unordered_map>
#include <set>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../globals.h"

// Legacy MVP struct used by the textured quad fallback path (Phase 4).
// Model rendering uses FrameUBO + ModelPushConstant instead (Phase 6).
struct QuadMVPUniform {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// ─── Lifecycle ───────────────────────────────────────────────────────────────

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

bool VulkanRenderer::init(VulkanContext* context) {
    ctx = context;

    // Must happen before createRenderPass() + createDepthResources() which
    // read msaaSamples. VMA allocator is created later in createTriangleResources(),
    // so createMSAAResources() is deferred until after that.
    msaaMaxSamples = getMaxUsableSampleCount();
    msaaSamples    = msaaMaxSamples;

    if (!createRenderPass())         return false;
    if (!createMSAAResources())      return false;  // needs no VMA — runs before framebuffers
    if (!createDepthResources())     return false;
    if (!createFramebuffers())       return false;
    if (!createCommandPool())        return false;
    if (!createCommandBuffers())     return false;
    if (!createSyncObjects())        return false;
    if (!createTriangleResources())  return false;
    if (!createDescriptorSets())     return false;
    if (!createShadowResources())    return false;
    if (!createSkyboxResources())    return false;
    if (!createGridResources())      return false;
    if (!createUIResources())        return false;
    if (!initImGui())                return false;
    // Water deferred: needs modelUniformBuffers, created in loadModel()/createModelPipelineAndDescriptors()

    std::cout << "[Vulkan] Renderer initialized" << std::endl;
    return true;
}

void VulkanRenderer::cleanup() {
    if (!ctx) return;
    VkDevice device = ctx->getDevice();
    if (!device) return;

    vkDeviceWaitIdle(device);

    // Sync objects — per-swapchain-image renderFinished semaphores
    for (auto& sem : renderFinishedSemaphores) {
        if (sem) vkDestroySemaphore(device, sem, nullptr);
    }
    renderFinishedSemaphores.clear();

    // Per-frame-in-flight semaphores + fences
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        if (inFlightFences[i])           vkDestroyFence(device, inFlightFences[i], nullptr);
        imageAvailableSemaphores[i] = nullptr;
        inFlightFences[i] = nullptr;
    }

    // Scene model resources — destroy all uploaded models
    if (allocator && !sceneModels.empty()) {
        for (auto& [model, data] : sceneModels) {
            std::set<VkImage> destroyedImages;
            for (auto& meshGPU : data->meshes) {
                if (meshGPU.diffuseTexture.image && destroyedImages.find(meshGPU.diffuseTexture.image) == destroyedImages.end()) {
                    destroyedImages.insert(meshGPU.diffuseTexture.image);
                    destroyTexture(allocator, device, meshGPU.diffuseTexture);
                }
                if (meshGPU.normalTexture.image && destroyedImages.find(meshGPU.normalTexture.image) == destroyedImages.end()) {
                    destroyedImages.insert(meshGPU.normalTexture.image);
                    destroyTexture(allocator, device, meshGPU.normalTexture);
                }
                if (meshGPU.metallicTexture.image && destroyedImages.find(meshGPU.metallicTexture.image) == destroyedImages.end()) {
                    destroyedImages.insert(meshGPU.metallicTexture.image);
                    destroyTexture(allocator, device, meshGPU.metallicTexture);
                }
                if (meshGPU.roughnessTexture.image && destroyedImages.find(meshGPU.roughnessTexture.image) == destroyedImages.end()) {
                    destroyedImages.insert(meshGPU.roughnessTexture.image);
                    destroyTexture(allocator, device, meshGPU.roughnessTexture);
                }
                // VkMeshData destructor handles buffer cleanup via its unique_ptr
            }
            for (auto& buf : data->boneBuffers) destroyBuffer(allocator, buf);
        }
        sceneModels.clear();
    }
    if (allocator) {
        destroyTexture(allocator, device, whiteTexture);
        for (auto& ub : modelUniformBuffers) destroyBuffer(allocator, ub);
        for (auto& ub : boneUniformBuffers) destroyBuffer(allocator, ub);
        if (groundVertexBuffer.buffer) destroyBuffer(allocator, groundVertexBuffer);
        if (groundIndexBuffer.buffer)  destroyBuffer(allocator, groundIndexBuffer);
    }
    if (modelPipeline)       { vkDestroyPipeline(device, modelPipeline, nullptr);             modelPipeline = nullptr; }
    if (modelPipelineLayout) { vkDestroyPipelineLayout(device, modelPipelineLayout, nullptr); modelPipelineLayout = nullptr; }
    if (modelDescriptorPool) { vkDestroyDescriptorPool(device, modelDescriptorPool, nullptr); modelDescriptorPool = nullptr; }
    if (modelDescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, modelDescriptorSetLayout, nullptr); modelDescriptorSetLayout = nullptr; }

    // PBR resources (Phase 14)
    if (allocator) {
        destroyTexture(allocator, device, flatNormalTexture);
        destroyTexture(allocator, device, blackTexture);
    }
    if (pbrPipeline)            { vkDestroyPipeline(device, pbrPipeline, nullptr);             pbrPipeline = nullptr; }
    if (pbrPipelineLayout)      { vkDestroyPipelineLayout(device, pbrPipelineLayout, nullptr); pbrPipelineLayout = nullptr; }
    if (pbrDescriptorPool)      { vkDestroyDescriptorPool(device, pbrDescriptorPool, nullptr); pbrDescriptorPool = nullptr; }
    if (pbrDescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, pbrDescriptorSetLayout, nullptr); pbrDescriptorSetLayout = nullptr; }

    // Grid resources (Phase 12)
    cleanupGridResources();

    // Grass resources (Phase 17)
    cleanupGrassResources();

    // Water resources (Phase 16)
    cleanupWaterResources();

    // UI text + sprite resources (Phase 15)
    cleanupUIResources();

    // ImGui resources (Phase 11)
    cleanupImGui();

    // ID buffer resources (Phase 11, Part 2)
    cleanupIDBufferResources();

    // Skybox resources (Phase 9)
    cleanupSkyboxResources();

    // Shadow resources (Phase 7)
    cleanupShadowResources();

    // Quad resources
    if (allocator) {
        destroyTexture(allocator, device, texture);
        destroyBuffer(allocator, vertexBuffer);
        destroyBuffer(allocator, indexBuffer);
        for (auto& ub : uniformBuffers) destroyBuffer(allocator, ub);
    }

    if (pipeline)       { vkDestroyPipeline(device, pipeline, nullptr);             pipeline = nullptr; }
    if (pipelineLayout) { vkDestroyPipelineLayout(device, pipelineLayout, nullptr); pipelineLayout = nullptr; }
    if (descriptorPool) { vkDestroyDescriptorPool(device, descriptorPool, nullptr); descriptorPool = nullptr; }
    if (descriptorSetLayout) { vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr); descriptorSetLayout = nullptr; }

    if (commandPool) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = nullptr;
    }

    cleanupFramebuffers();
    cleanupDepthResources();
    cleanupMSAAResources();

    if (renderPass) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = nullptr;
    }

    if (allocator) {
        vmaDestroyAllocator(allocator);
        allocator = nullptr;
    }

    ctx = nullptr;
}

void VulkanRenderer::setClearColor(float r, float g, float b, float a) {
    clearColor = {r, g, b, a};
}

// ─── Render Pass ─────────────────────────────────────────────────────────────
//
// A render pass describes WHAT attachments (images) the GPU will write to and
// HOW they should be treated. Think of it as a "recipe" for a rendering step:
//   - Color attachment: the swapchain image we'll present
//   - Depth attachment: depth buffer for 3D ordering
//
// loadOp/storeOp control whether data is cleared/preserved between frames.
// initialLayout/finalLayout describe image state transitions.

bool VulkanRenderer::createRenderPass() {
    depthFormat = findDepthFormat();
    const bool msaa = (msaaSamples != VK_SAMPLE_COUNT_1_BIT);

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    if (!msaa) {
        // ── Non-MSAA path (1-sample): color + depth ──────────────────────────
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = ctx->getSwapchainFormat();
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format         = depthFormat;
        depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        rpInfo.pAttachments    = attachments.data();
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dependency;

        if (vkCreateRenderPass(ctx->getDevice(), &rpInfo, nullptr, &renderPass) != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to create render pass" << std::endl;
            return false;
        }
    } else {
        // ── MSAA path: MSAA color (0) + MSAA depth (1) + resolve (2) ─────────
        // Attachment 0: MSAA color buffer (rendered into, then resolved)
        VkAttachmentDescription msaaColor{};
        msaaColor.format         = ctx->getSwapchainFormat();
        msaaColor.samples        = msaaSamples;
        msaaColor.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        msaaColor.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE; // discarded after resolve
        msaaColor.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        msaaColor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        msaaColor.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        msaaColor.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Attachment 1: MSAA depth buffer
        VkAttachmentDescription msaaDepth{};
        msaaDepth.format         = depthFormat;
        msaaDepth.samples        = msaaSamples;
        msaaDepth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        msaaDepth.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        msaaDepth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        msaaDepth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        msaaDepth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        msaaDepth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Attachment 2: resolve target — 1-sample swapchain image for presentation
        VkAttachmentDescription resolve{};
        resolve.format         = ctx->getSwapchainFormat();
        resolve.samples        = VK_SAMPLE_COUNT_1_BIT;
        resolve.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        resolve.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolve.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        resolve.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef  {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef  {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkAttachmentReference resolveRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;
        subpass.pResolveAttachments     = &resolveRef;

        std::array<VkAttachmentDescription, 3> attachments = {msaaColor, msaaDepth, resolve};
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        rpInfo.pAttachments    = attachments.data();
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dependency;

        if (vkCreateRenderPass(ctx->getDevice(), &rpInfo, nullptr, &renderPass) != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to create MSAA render pass" << std::endl;
            return false;
        }
    }

    std::cout << "[Vulkan] Render pass created (samples=" << msaaSamples << ")" << std::endl;
    return true;
}

// ─── Depth Resources ─────────────────────────────────────────────────────────

VkFormat VulkanRenderer::findDepthFormat() {
    // Try formats in order of preference
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(ctx->getPhysicalDevice(), format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    std::cerr << "[Vulkan] Failed to find supported depth format" << std::endl;
    return VK_FORMAT_D32_SFLOAT; // fallback
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(ctx->getPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    std::cerr << "[Vulkan] Failed to find suitable memory type" << std::endl;
    return 0;
}

bool VulkanRenderer::createDepthResources() {
    VkExtent2D extent = ctx->getSwapchainExtent();
    VkDevice device = ctx->getDevice();

    // Create depth image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = depthFormat;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples       = msaaSamples;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &depthImage) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create depth image" << std::endl;
        return false;
    }

    // Allocate memory for the depth image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, depthImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &depthImageMemory) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to allocate depth image memory" << std::endl;
        return false;
    }

    vkBindImageMemory(device, depthImage, depthImageMemory, 0);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = depthImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = depthFormat;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create depth image view" << std::endl;
        return false;
    }

    return true;
}

void VulkanRenderer::cleanupDepthResources() {
    VkDevice device = ctx ? ctx->getDevice() : nullptr;
    if (!device) return;

    if (depthImageView)   { vkDestroyImageView(device, depthImageView, nullptr);   depthImageView = nullptr; }
    if (depthImage)       { vkDestroyImage(device, depthImage, nullptr);           depthImage = nullptr; }
    if (depthImageMemory) { vkFreeMemory(device, depthImageMemory, nullptr);       depthImageMemory = nullptr; }
}

// ─── MSAA Resources ───────────────────────────────────────────────────────────

VkSampleCountFlagBits VulkanRenderer::getMaxUsableSampleCount() {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(ctx->getPhysicalDevice(), &props);

    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts &
                                props.limits.framebufferDepthSampleCounts;

    // Cap at 4x — 8x/16x give diminishing returns and cost more memory/bandwidth
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

bool VulkanRenderer::createMSAAResources() {
    if (msaaSamples == VK_SAMPLE_COUNT_1_BIT) return true;

    VkDevice   device = ctx->getDevice();
    VkExtent2D extent = ctx->getSwapchainExtent();
    VkFormat   format = ctx->getSwapchainFormat();

    // Plain vkCreateImage — no VMA needed, so this can run before createTriangleResources()
    VkImageCreateInfo imgInfo{};
    imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType     = VK_IMAGE_TYPE_2D;
    imgInfo.format        = format;
    imgInfo.extent        = {extent.width, extent.height, 1};
    imgInfo.mipLevels     = 1;
    imgInfo.arrayLayers   = 1;
    imgInfo.samples       = msaaSamples;
    imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imgInfo, nullptr, &msaaColorImage) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create MSAA color image" << std::endl;
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, msaaColorImage, &memReqs);

    // Prefer lazily-allocated memory (Metal tile memory on Apple Silicon)
    VkMemoryPropertyFlags preferred = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT |
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryPropertyFlags fallback  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(ctx->getPhysicalDevice(), &memProps);

    auto findMemType = [&](VkMemoryPropertyFlags flags) -> uint32_t {
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((memReqs.memoryTypeBits & (1u << i)) &&
                (memProps.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        }
        return UINT32_MAX;
    };

    uint32_t memIdx = findMemType(preferred);
    if (memIdx == UINT32_MAX) memIdx = findMemType(fallback);
    if (memIdx == UINT32_MAX) {
        std::cerr << "[Vulkan] No suitable memory type for MSAA image" << std::endl;
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = memIdx;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &msaaColorMemory) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to allocate MSAA color memory" << std::endl;
        return false;
    }
    vkBindImageMemory(device, msaaColorImage, msaaColorMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = msaaColorImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &msaaColorView) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create MSAA color image view" << std::endl;
        return false;
    }

    std::cout << "[Vulkan] MSAA color image created (" << msaaSamples << "x)" << std::endl;
    return true;
}

void VulkanRenderer::cleanupMSAAResources() {
    if (!ctx) return;
    VkDevice device = ctx->getDevice();
    if (msaaColorView)   { vkDestroyImageView(device, msaaColorView, nullptr);  msaaColorView   = nullptr; }
    if (msaaColorImage)  { vkDestroyImage(device, msaaColorImage, nullptr);     msaaColorImage  = nullptr; }
    if (msaaColorMemory) { vkFreeMemory(device, msaaColorMemory, nullptr);      msaaColorMemory = nullptr; }
}

// ─── Live MSAA toggle ─────────────────────────────────────────────────────────
//
// Rebuilds: render pass → MSAA image → depth → framebuffers → all main-pass pipelines.
// Shadow and ID pipelines keep their own render passes and stay 1-sample.

bool VulkanRenderer::recreateForMSAAChange(VkSampleCountFlagBits newSamples) {
    VkDevice device = ctx->getDevice();
    vkDeviceWaitIdle(device);

    // ── Tear down render-pass dependents ─────────────────────────────────────
    cleanupFramebuffers();
    cleanupDepthResources();
    cleanupMSAAResources();
    if (renderPass) { vkDestroyRenderPass(device, renderPass, nullptr); renderPass = nullptr; }

    // ── Destroy all main-pass VkPipeline + VkPipelineLayout objects ──────────
    auto destroyPL = [&](VkPipeline& p, VkPipelineLayout& l) {
        if (p) { vkDestroyPipeline(device, p, nullptr); p = nullptr; }
        if (l) { vkDestroyPipelineLayout(device, l, nullptr); l = nullptr; }
    };
    destroyPL(pipeline,         pipelineLayout);
    destroyPL(modelPipeline,    modelPipelineLayout);
    destroyPL(pbrPipeline,      pbrPipelineLayout);
    destroyPL(skyboxPipeline,   skyboxPipelineLayout);
    destroyPL(idDebugPipeline,  idDebugPipelineLayout);
    destroyPL(gridPipeline,     gridPipelineLayout);
    destroyPL(grassPipeline,    grassPipelineLayout);
    destroyPL(waterPipeline,    waterPipelineLayout);
    destroyPL(uiTextPipeline,   uiTextPipelineLayout);
    destroyPL(uiSpritePipeline, uiSpritePipelineLayout);

    // ── Apply new sample count and rebuild ────────────────────────────────────
    msaaSamples = newSamples;

    if (!createRenderPass())    return false;
    if (!createMSAAResources()) return false;
    if (!createDepthResources()) return false;
    if (!createFramebuffers())  return false;

    // Recreate pipelines that have valid descriptor set layouts
    if (descriptorSetLayout)
        createTexturedPipeline(device, renderPass, descriptorSetLayout,
                               pipelineLayout, pipeline, msaaSamples);
    if (modelDescriptorSetLayout)
        createModelPipeline(device, renderPass, modelDescriptorSetLayout,
                            modelPipelineLayout, modelPipeline, msaaSamples);
    if (pbrDescriptorSetLayout)
        createPBRPipeline(device, renderPass, pbrDescriptorSetLayout,
                          pbrPipelineLayout, pbrPipeline, msaaSamples);
    if (skyboxDescriptorSetLayout)
        createSkyboxPipeline(device, renderPass, skyboxDescriptorSetLayout,
                             skyboxPipelineLayout, skyboxPipeline, msaaSamples);
    if (idDescriptorSetLayout)
        createIDDebugPipeline(device, renderPass, idDescriptorSetLayout,
                              idDebugPipelineLayout, idDebugPipeline, msaaSamples);
    if (gridDescriptorSetLayout)
        createGridPipeline(device, renderPass, gridDescriptorSetLayout,
                           gridPipelineLayout, gridPipeline, msaaSamples);
    if (grassDescriptorSetLayout)
        createGrassPipeline(device, renderPass, grassDescriptorSetLayout,
                            grassPipelineLayout, grassPipeline, msaaSamples);
    if (waterDescriptorSetLayout)
        createWaterPipeline(device, renderPass, waterDescriptorSetLayout,
                            waterPipelineLayout, waterPipeline, msaaSamples);
    if (uiDescriptorSetLayout) {
        createUITextPipeline(device, renderPass, uiDescriptorSetLayout,
                             uiTextPipelineLayout, uiTextPipeline, msaaSamples);
        createUISpritePipeline(device, renderPass, uiDescriptorSetLayout,
                               uiSpritePipelineLayout, uiSpritePipeline, msaaSamples);
    }

    // ImGui render pass must match — reinit only the Vulkan backend.
    // The ImGuiContext and the GLFW backend stay alive; only the Vk renderer
    // pipeline (which depends on the render pass) needs to be rebuilt.
    if (imguiInitialized) {
        ImGuiContext* prevCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiContext);

        ImGui_ImplVulkan_Shutdown();  // destroys Vulkan pipeline + descriptor pool

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion       = VK_API_VERSION_1_0;
        initInfo.Instance         = ctx->getInstance();
        initInfo.PhysicalDevice   = ctx->getPhysicalDevice();
        initInfo.Device           = device;
        initInfo.QueueFamily      = ctx->getGraphicsQueueFamily();
        initInfo.Queue            = ctx->getGraphicsQueue();
        initInfo.DescriptorPool   = VK_NULL_HANDLE; // backend creates its own
        initInfo.DescriptorPoolSize = 1000;
        initInfo.RenderPass       = renderPass;
        initInfo.MinImageCount    = 2;
        initInfo.ImageCount       = static_cast<uint32_t>(ctx->getSwapchainImages().size());
        initInfo.MSAASamples      = VK_SAMPLE_COUNT_1_BIT;
        initInfo.Subpass          = 0;
        ImGui_ImplVulkan_Init(&initInfo);

        ImGui::SetCurrentContext(prevCtx);
    }

    std::cout << "[Vulkan] MSAA changed to " << msaaSamples << "x" << std::endl;
    return true;
}

// ─── Framebuffers ────────────────────────────────────────────────────────────
//
// A framebuffer binds actual images (VkImageView) to a render pass's attachment
// slots. We need one framebuffer per swapchain image because each has a
// different color image, but they all share the same depth image.

bool VulkanRenderer::createFramebuffers() {
    const auto& imageViews = ctx->getSwapchainImageViews();
    VkExtent2D extent = ctx->getSwapchainExtent();
    const bool msaa = (msaaSamples != VK_SAMPLE_COUNT_1_BIT);

    framebuffers.resize(imageViews.size());

    for (size_t i = 0; i < imageViews.size(); i++) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType      = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.width      = extent.width;
        fbInfo.height     = extent.height;
        fbInfo.layers     = 1;

        if (msaa) {
            // MSAA: attachment 0=MSAA color, 1=MSAA depth, 2=resolve (swapchain)
            std::array<VkImageView, 3> attachments = {
                msaaColorView,  // attachment 0 — MSAA color
                depthImageView, // attachment 1 — MSAA depth
                imageViews[i],  // attachment 2 — resolve target
            };
            fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            fbInfo.pAttachments    = attachments.data();
        } else {
            std::array<VkImageView, 2> attachments = {
                imageViews[i],  // attachment 0 — color
                depthImageView, // attachment 1 — depth
            };
            fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            fbInfo.pAttachments    = attachments.data();
        }

        if (vkCreateFramebuffer(ctx->getDevice(), &fbInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to create framebuffer " << i << std::endl;
            return false;
        }
    }

    std::cout << "[Vulkan] Created " << framebuffers.size() << " framebuffers" << std::endl;
    return true;
}

void VulkanRenderer::cleanupFramebuffers() {
    VkDevice device = ctx ? ctx->getDevice() : nullptr;
    if (!device) return;

    for (auto fb : framebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    framebuffers.clear();
}

// ─── Command Pool & Buffers ──────────────────────────────────────────────────
//
// Commands in Vulkan are not executed immediately. Instead, you record them
// into a command buffer, then submit the whole buffer to a queue.
//
// Command pool: allocator tied to a specific queue family.
// Command buffer: a recording of GPU commands (begin pass, draw, end pass...).
// We create one per frame-in-flight so we can record the next frame while the
// GPU is still working on the previous one.

bool VulkanRenderer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = ctx->getGraphicsQueueFamily();

    if (vkCreateCommandPool(ctx->getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create command pool" << std::endl;
        return false;
    }

    return true;
}

bool VulkanRenderer::createCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(ctx->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to allocate command buffers" << std::endl;
        return false;
    }

    return true;
}

// ─── Synchronization ─────────────────────────────────────────────────────────
//
// Vulkan gives you explicit control over GPU/CPU synchronization (unlike OpenGL
// which does it behind the scenes). Three primitives:
//
//   Semaphore (GPU-GPU): signals between queue operations.
//     - imageAvailable: swapchain image has been acquired, safe to render
//     - renderFinished: rendering is done, safe to present
//
//   Fence (GPU-CPU): lets the CPU wait for the GPU to finish.
//     - inFlightFence: prevents the CPU from recording into a command buffer
//       that the GPU is still executing
//
// We create one set per frame-in-flight so frame N and frame N+1 don't
// interfere with each other.

bool VulkanRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first frame doesn't deadlock

    VkDevice device = ctx->getDevice();

    // Per-frame-in-flight: imageAvailable semaphores + fences
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to create sync objects for frame " << i << std::endl;
            return false;
        }
    }

    // Per-swapchain-image: renderFinished semaphores
    // This prevents the semaphore reuse issue when swapchain has more images
    // than frames-in-flight (e.g. 3 images, 2 FIF).
    uint32_t imageCount = static_cast<uint32_t>(ctx->getSwapchainImages().size());
    renderFinishedSemaphores.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to create renderFinished semaphore " << i << std::endl;
            return false;
        }
    }

    std::cout << "[Vulkan] Sync objects created (" << MAX_FRAMES_IN_FLIGHT
              << " frames in flight, " << imageCount << " swapchain images)" << std::endl;
    return true;
}

// ─── Draw Frame ──────────────────────────────────────────────────────────────
//
// This is the core frame loop. Every frame follows this sequence:
//
//   1. Wait for the previous frame using this slot to finish (fence)
//   2. Acquire the next swapchain image (semaphore signals when ready)
//   3. Record commands into the command buffer
//   4. Submit the command buffer to the graphics queue
//   5. Present the rendered image to the screen
//
// The semaphores ensure GPU-side ordering (acquire → render → present).
// The fence ensures the CPU doesn't overwrite a command buffer still in use.

bool VulkanRenderer::drawFrame() {
    VkDevice device = ctx->getDevice();

    // FPS calculation
    {
        double now = glfwGetTime();
        double dt  = now - lastFrameTime;
        lastFrameTime = now;
        displayFPS = (dt > 0.0) ? static_cast<float>(1.0 / dt) : 0.0f;
    }
    // Reset per-frame counters (shared globals, same as OpenGL path)
    vkDrawCalls = 0;

    // Apply pending MSAA toggle (safe here — GPU is about to wait on the fence anyway)
    if (pendingMSAAToggle) {
        pendingMSAAToggle = false;
        VkSampleCountFlagBits next = (msaaSamples == VK_SAMPLE_COUNT_1_BIT)
                                     ? msaaMaxSamples : VK_SAMPLE_COUNT_1_BIT;
        if (next != msaaSamples) recreateForMSAAChange(next);
    }

    // 1. Wait for previous frame using this slot
    vkWaitForFences(device, 1, &inFlightFences[currentFrame],
                    VK_TRUE, std::numeric_limits<uint64_t>::max());

    // 2. Acquire next swapchain image
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device, ctx->getSwapchain(),
        std::numeric_limits<uint64_t>::max(),
        imageAvailableSemaphores[currentFrame], // Signal this when image is ready
        VK_NULL_HANDLE,
        &imageIndex);

    // Handle swapchain out-of-date (window resized)
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return false; // Caller should recreate swapchain
    }

    // Only reset fence after we know we'll submit work (avoid deadlock)
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    // 3. Record command buffer
    VkCommandBuffer cmd = commandBuffers[currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // ── Bone animation (Phase 10) ──────────────────────────────────────────
    // Update per-model bone buffers — must happen before shadow and main passes.
    for (auto& [model, data] : sceneModels) {
        BoneUBO boneUbo{};  // zero-initialized = identity, used by static meshes
        if (model->IsAnimated()) {
            std::vector<glm::mat4> transforms;
            model->GetBoneTransforms(transforms, static_cast<float>(glfwGetTime()));
            size_t count = std::min(transforms.size(), static_cast<size_t>(MAX_BONES));
            memcpy(boneUbo.bones, transforms.data(), count * sizeof(glm::mat4));
        }
        void* boneMapped;
        vmaMapMemory(allocator, data->boneBuffers[currentFrame].allocation, &boneMapped);
        memcpy(boneMapped, &boneUbo, sizeof(boneUbo));
        vmaUnmapMemory(allocator, data->boneBuffers[currentFrame].allocation);
    }

    // ── Shadow pass (Phase 7) ───────────────────────────────────────────────
    // Compute light-space matrix (directional light, matching OpenGL setup)
    glm::vec3 lightPos(-1.0f, 4.0f, 1.0f);
    float near_plane = 1.0f, far_plane = 75.5f;
    glm::mat4 lightProjection = glm::ortho(-25.0f, 25.0f, -25.0f, 25.0f, near_plane, far_plane);
    // Convert GLM [-1,1] depth to Vulkan [0,1] depth
    lightProjection[2][2] *= 0.5f;
    lightProjection[3][2] = lightProjection[3][2] * 0.5f + 0.5f;
    lightProjection[1][1] *= -1; // Vulkan NDC Y-flip
    glm::mat4 lightView       = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    if (!sceneModels.empty() && shadowRenderPass && currentScene) {
        // Update shadow UBO
        LightUBO lightUbo{};
        lightUbo.lightSpaceMatrix = lightSpaceMatrix;

        void* shadowMapped;
        vmaMapMemory(allocator, shadowUniformBuffers[currentFrame].allocation, &shadowMapped);
        memcpy(shadowMapped, &lightUbo, sizeof(lightUbo));
        vmaUnmapMemory(allocator, shadowUniformBuffers[currentFrame].allocation);

        // Begin shadow render pass
        VkClearValue shadowClear{};
        shadowClear.depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo shadowRPInfo{};
        shadowRPInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        shadowRPInfo.renderPass        = shadowRenderPass;
        shadowRPInfo.framebuffer       = shadowFramebuffer;
        shadowRPInfo.renderArea.offset = {0, 0};
        shadowRPInfo.renderArea.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};
        shadowRPInfo.clearValueCount   = 1;
        shadowRPInfo.pClearValues      = &shadowClear;

        vkCmdBeginRenderPass(cmd, &shadowRPInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport shadowViewport{};
        shadowViewport.width    = static_cast<float>(SHADOW_MAP_SIZE);
        shadowViewport.height   = static_cast<float>(SHADOW_MAP_SIZE);
        shadowViewport.minDepth = 0.0f;
        shadowViewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &shadowViewport);

        VkRect2D shadowScissor{};
        shadowScissor.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};
        vkCmdSetScissor(cmd, 0, 1, &shadowScissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
        // Shadow descriptor sets use renderer-level boneUniformBuffers (identity bones)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipelineLayout,
                                0, 1, &shadowDescriptorSets[currentFrame], 0, nullptr);

        // Draw all scene objects
        for (auto& obj : currentScene->getGameObjects()) {
            if (!obj->model) continue;
            auto it = sceneModels.find(obj->model.get());
            if (it == sceneModels.end()) {
                loadModel(obj->model.get());
                it = sceneModels.find(obj->model.get());
                if (it == sceneModels.end()) continue;
            }

            ModelPushConstant shadowPush{};
            shadowPush.model = obj->getModelMatrix();
            vkCmdPushConstants(cmd, shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(ModelPushConstant), &shadowPush);

            for (auto& meshGPU : it->second->meshes) {
                VkBuffer vbufs[] = {meshGPU.buffers->vertexBuffer.buffer};
                VkDeviceSize voffs[] = {0};
                vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, voffs);
                vkCmdBindIndexBuffer(cmd, meshGPU.buffers->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, meshGPU.buffers->getIndexCount(), 1, 0, 0, 0);
            }
        }

        // Ground plane in shadow pass (identity transform)
        if (groundVertexBuffer.buffer) {
            ModelPushConstant groundPush{};
            groundPush.model = glm::mat4(1.0f);
            vkCmdPushConstants(cmd, shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(ModelPushConstant), &groundPush);

            VkBuffer gvb[] = {groundVertexBuffer.buffer};
            VkDeviceSize gvo[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, gvb, gvo);
            vkCmdBindIndexBuffer(cmd, groundIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(cmd);
    }

    // ── Reflection clear pass (Phase 16) ────────────────────────────────────
    // Begin + immediately end the reflection render pass so the image gets
    // cleared to sky-blue and transitioned to SHADER_READ_ONLY_OPTIMAL.
    // A full scene re-render into reflectionFramebuffer would go here.
    if (showWater && reflectionRenderPass && reflectionFramebuffer) {
        VkExtent2D refExt = { std::max(1u, ctx->getSwapchainExtent().width  / 2),
                              std::max(1u, ctx->getSwapchainExtent().height / 2) };
        std::array<VkClearValue, 2> refClear{};
        refClear[0].color        = {{ 0.53f, 0.81f, 0.92f, 1.0f }};  // sky blue
        refClear[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo refBegin{};
        refBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        refBegin.renderPass        = reflectionRenderPass;
        refBegin.framebuffer       = reflectionFramebuffer;
        refBegin.renderArea.extent = refExt;
        refBegin.clearValueCount   = 2;
        refBegin.pClearValues      = refClear.data();
        vkCmdBeginRenderPass(cmd, &refBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(cmd);  // just clears and transitions the image
    }

    // ── Main render pass ────────────────────────────────────────────────────
    // Begin render pass with clear values
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpBeginInfo{};
    rpBeginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass        = renderPass;
    rpBeginInfo.framebuffer       = framebuffers[imageIndex];
    rpBeginInfo.renderArea.offset = {0, 0};
    rpBeginInfo.renderArea.extent = ctx->getSwapchainExtent();
    rpBeginInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    rpBeginInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Dynamic viewport and scissor (shared by both paths)
    VkExtent2D extent = ctx->getSwapchainExtent();
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(extent.width);
    viewport.height   = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    if (!sceneModels.empty() && currentScene) {
        // ── Scene rendering path ─────────────────────────────────────────────
        // Update per-frame UBO (view, proj, light data, camera position)
        FrameUBO frameUbo{};
        frameUbo.view             = currentView;
        frameUbo.proj             = currentProj;
        frameUbo.proj[1][1]      *= -1; // Vulkan NDC Y-flip
        frameUbo.lightSpaceMatrix = lightSpaceMatrix;
        frameUbo.lightPos         = glm::vec4(lightPos, 1.0f);
        // Camera position in world space = last column of inverse view
        frameUbo.viewPos = glm::inverse(currentView) * glm::vec4(0, 0, 0, 1);

        // Scene lights
        frameUbo.numPointLights = 0;

        void* mapped;
        vmaMapMemory(allocator, modelUniformBuffers[currentFrame].allocation, &mapped);
        memcpy(mapped, &frameUbo, sizeof(frameUbo));
        vmaUnmapMemory(allocator, modelUniformBuffers[currentFrame].allocation);

        // Draw all scene objects
        for (auto& obj : currentScene->getGameObjects()) {
            if (!obj->model) continue;
            auto it = sceneModels.find(obj->model.get());
            if (it == sceneModels.end()) {
                loadModel(obj->model.get());
                it = sceneModels.find(obj->model.get());
                if (it == sceneModels.end()) continue;
            }

            glm::mat4 objTransform = obj->getModelMatrix();

            // Draw each mesh — choose PBR or simple pipeline per mesh
            for (auto& meshGPU : it->second->meshes) {
                VkBuffer vbuffers[] = {meshGPU.buffers->vertexBuffer.buffer};
                VkDeviceSize voffsets[] = {0};
                vkCmdBindVertexBuffers(cmd, 0, 1, vbuffers, voffsets);
                vkCmdBindIndexBuffer(cmd, meshGPU.buffers->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

                if (meshGPU.hasPBR && pbrPipeline && meshGPU.pbrDescriptorSets[currentFrame] != VK_NULL_HANDLE) {
                    // PBR path
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipeline);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipelineLayout,
                                            0, 1, &meshGPU.pbrDescriptorSets[currentFrame], 0, nullptr);

                    PBRPushConstant pbrPush{};
                    pbrPush.model        = objTransform;
                    pbrPush.metallicVal  = 1.0f;
                    pbrPush.roughnessVal = 1.0f;
                    pbrPush.hasNormalMap = meshGPU.normalTexture.image ? 1u : 0u;
                    vkCmdPushConstants(cmd, pbrPipelineLayout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0, sizeof(PBRPushConstant), &pbrPush);
                } else {
                    // Simple diffuse path
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipeline);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipelineLayout,
                                            0, 1, &meshGPU.descriptorSets[currentFrame], 0, nullptr);

                    ModelPushConstant push{};
                    push.model = objTransform;
                    vkCmdPushConstants(cmd, modelPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                                       0, sizeof(ModelPushConstant), &push);
                }
                uint32_t ic = meshGPU.buffers->getIndexCount();
                vkCmdDrawIndexed(cmd, ic, 1, 0, 0, 0);
                vkDrawCalls++;
                drawCalls++;
                trianglesDrawn += ic / 3;
                verticesDrawn  += ic;
            }
        }  // end obj loop

        // Ground plane — always simple pipeline
        if (groundVertexBuffer.buffer) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipeline);
            ModelPushConstant groundPush{};
            groundPush.model = glm::mat4(1.0f);
            vkCmdPushConstants(cmd, modelPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(ModelPushConstant), &groundPush);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipelineLayout,
                                    0, 1, &groundDescriptorSets[currentFrame], 0, nullptr);
            VkBuffer gvb[] = {groundVertexBuffer.buffer};
            VkDeviceSize gvo[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, gvb, gvo);
            vkCmdBindIndexBuffer(cmd, groundIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
        }
    } else {
        // ── Textured quad fallback (Phase 4) ────────────────────────────────
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        VkBuffer quadVBufs[] = {vertexBuffer.buffer};
        VkDeviceSize quadOffsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, quadVBufs, quadOffsets);
        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

        // Update quad UBO
        float time = static_cast<float>(glfwGetTime());
        float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);

        QuadMVPUniform ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj  = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        void* mapped;
        vmaMapMemory(allocator, uniformBuffers[currentFrame].allocation, &mapped);
        memcpy(mapped, &ubo, sizeof(ubo));
        vmaUnmapMemory(allocator, uniformBuffers[currentFrame].allocation);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                0, 1, &descriptorSets[currentFrame], 0, nullptr);

        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    }

    // ── Skybox (Phase 9) — drawn last, depth test LEQUAL culls behind geometry ─
    if (hasSkybox && skyboxPipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);

        // Push viewProj with translation removed from view matrix
        glm::mat4 skyView = glm::mat4(glm::mat3(currentView)); // strip translation
        glm::mat4 skyProj = currentProj;
        skyProj[1][1] *= -1; // Vulkan Y-flip
        glm::mat4 viewProj = skyProj * skyView;
        vkCmdPushConstants(cmd, skyboxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(glm::mat4), &viewProj);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout,
                                0, 1, &skyboxDescriptorSet, 0, nullptr);

        VkBuffer skyVB[] = {skyboxVertexBuffer.buffer};
        VkDeviceSize skyOff[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, skyVB, skyOff);
        vkCmdDraw(cmd, 36, 1, 0, 0);
    }

    // ── Grass field (Phase 17) ───────────────────────────────────────────────
    if (showGrass && grassPipeline && grassBladeVertexBuffer.buffer && grassInstanceCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grassPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grassPipelineLayout,
                                0, 1, &grassDescriptorSets[currentFrame], 0, nullptr);

        struct GrassPush { float time, windStrength, windSpeed, grassHeight; } gp;
        static float grassTime = 0.0f;
        grassTime += 0.016f;
        gp = { grassTime, 0.3f, 1.2f, 0.0f };
        vkCmdPushConstants(cmd, grassPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(gp), &gp);

        VkBuffer     vbufs[2]  = { grassBladeVertexBuffer.buffer, grassInstanceBuffer.buffer };
        VkDeviceSize offsets[2] = { 0, 0 };
        vkCmdBindVertexBuffers(cmd, 0, 2, vbufs, offsets);
        vkCmdBindIndexBuffer(cmd, grassBladeIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, grassBladeIndexCount, grassInstanceCount, 0, 0, 0);
    }

    // ── Water surface (Phase 16) ─────────────────────────────────────────────
    if (showWater && waterPipeline && waterVertexBuffer.buffer) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, waterPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, waterPipelineLayout,
                                0, 1, &waterDescriptorSets[currentFrame], 0, nullptr);

        struct WaterPush { float time; float waveHeight; float waveSpeed; float waterLevel; } wp;
        static float waterTime = 0.0f;
        waterTime += 0.016f;  // ~60fps accumulation; real dt could be passed instead
        wp = { waterTime, 0.4f, 0.3f, -1.5f };  // waterLevel=-1.5 keeps water below ground/grass
        vkCmdPushConstants(cmd, waterPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(wp), &wp);

        VkBuffer wb = waterVertexBuffer.buffer;
        VkDeviceSize woff = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &wb, &woff);
        vkCmdBindIndexBuffer(cmd, waterIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, waterIndexCount, 1, 0, 0, 0);
    }

    // ── Infinite grid (Phase 12) ─────────────────────────────────────────────
    if (showGrid && gridPipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipelineLayout,
                                0, 1, &gridDescriptorSets[currentFrame], 0, nullptr);

        // Push camera position as vec4 (16 bytes)
        glm::vec4 camPos = glm::inverse(currentView) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        vkCmdPushConstants(cmd, gridPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(glm::vec4), &camPos);

        vkCmdDraw(cmd, 3, 1, 0, 0);  // Fullscreen triangle, no vertex buffer
    }

    // ── ID debug overlay (Phase 11, Part 2) ──────────────────────────────────
    // Re-renders all objects with semi-transparent false colors based on object ID
    if (showIDDebugOverlay && !sceneModels.empty() && currentScene) {
        // Lazy-create the debug pipeline if needed
        if (!idDebugPipeline) {
            if (!idDescriptorSetLayout) createIDBufferResources();
            if (idDescriptorSetLayout) {
                createIDDebugPipeline(ctx->getDevice(), renderPass, idDescriptorSetLayout,
                                      idDebugPipelineLayout, idDebugPipeline, msaaSamples);
            }
        }

        if (idDebugPipeline && idDescriptorSetLayout) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, idDebugPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, idDebugPipelineLayout,
                                    0, 1, &idDescriptorSets[currentFrame], 0, nullptr);

            uint32_t objIdx = 0;
            for (auto& obj : currentScene->getGameObjects()) {
                if (!obj->model) { objIdx++; continue; }
                auto it = sceneModels.find(obj->model.get());
                if (it == sceneModels.end()) { objIdx++; continue; }

                for (auto& meshGPU : it->second->meshes) {
                    IDPushConstant idPush{};
                    idPush.model    = obj->getModelMatrix();
                    idPush.objectID = objIdx + 2;
                    vkCmdPushConstants(cmd, idDebugPipelineLayout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0, sizeof(IDPushConstant), &idPush);

                    VkBuffer vbufs[] = {meshGPU.buffers->vertexBuffer.buffer};
                    VkDeviceSize voffs[] = {0};
                    vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, voffs);
                    vkCmdBindIndexBuffer(cmd, meshGPU.buffers->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(cmd, meshGPU.buffers->getIndexCount(), 1, 0, 0, 0);
                }
                objIdx++;
            }

            // Draw ground plane (ID = 1)
            if (groundVertexBuffer.buffer) {
                IDPushConstant groundPush{};
                groundPush.model    = glm::mat4(1.0f);
                groundPush.objectID = 1;
                vkCmdPushConstants(cmd, idDebugPipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(IDPushConstant), &groundPush);

                VkBuffer gvb[] = {groundVertexBuffer.buffer};
                VkDeviceSize gvo[] = {0};
                vkCmdBindVertexBuffers(cmd, 0, 1, gvb, gvo);
                vkCmdBindIndexBuffer(cmd, groundIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
            }
        }
    }

    // ── UI text overlay (Phase 15) ───────────────────────────────────────────
    if (hasUIResources) {
        renderText(cmd, "Hephaestus Engine", 12.0f, 28.0f, 0.45f, glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
    }

    // ── ImGui + ImGuizmo overlay (Phase 11 / Phase 13) ─────────────────────
    if (imguiInitialized) {
        ImGuiContext* prevCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiContext);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        // Set gizmo rect to full window
        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        ImGuizmo::SetOrthographic(false);

        ImGui::Begin("Vulkan Info");
        ImGui::Text("Renderer: Vulkan");

        // ── Performance stats ───────────────────────────────────────────────
        ImGui::Separator();
        ImGui::Text("FPS: %.1f  (%.2f ms)", displayFPS, displayFPS > 0 ? 1000.0f / displayFPS : 0.0f);
        ImGui::Text("Draw calls: %u", vkDrawCalls);
        ImGui::Text("Triangles:  %u", trianglesDrawn);
        ImGui::Text("Vertices:   %u", verticesDrawn);

        // ── MSAA ───────────────────────────────────────────────────────────
        ImGui::Separator();
        {
            bool msaaOn = (msaaSamples != VK_SAMPLE_COUNT_1_BIT);
            if (ImGui::Checkbox("MSAA", &msaaOn))
                pendingMSAAToggle = true;
            ImGui::SameLine();
            ImGui::Text("(%dx)", static_cast<int>(msaaSamples));
        }

        // ── Scene stats ────────────────────────────────────────────────────
        if (!sceneModels.empty()) {
            ImGui::Separator();
            int totalMeshes = 0, pbrCount = 0;
            for (auto& [model, data] : sceneModels)
                for (const auto& m : data->meshes) { totalMeshes++; if (m.hasPBR) pbrCount++; }
            ImGui::Text("Models: %zu  Meshes: %d  PBR: %d", sceneModels.size(), totalMeshes, pbrCount);
            ImGui::Text("Scene objects: %zu", currentScene ? currentScene->getGameObjects().size() : 0);
        }

        // ── Scene toggles ──────────────────────────────────────────────────
        ImGui::Separator();
        ImGui::Checkbox("Show Grid", &showGrid);
        ImGui::Checkbox("Show Grass", &showGrass);
        if (grassPipeline)
            ImGui::TextColored({0.4f,1.0f,0.4f,1.0f}, "Grass: %u inst, %u idx", grassInstanceCount, grassBladeIndexCount);
        else
            ImGui::TextColored({1.0f,0.4f,0.4f,1.0f}, "Grass: pipeline NULL");
        ImGui::Checkbox("Show Water", &showWater);
        ImGui::Checkbox("Show ID Debug Overlay", &showIDDebugOverlay);

        // ── Picking + gizmo ────────────────────────────────────────────────
        ImGui::Separator();
        ImGui::Text("Click to pick entity");
        if (selectedObjectIndex >= 0) {
            ImGui::TextColored({0.4f,1.0f,0.4f,1.0f}, "Selected: obj %d", selectedObjectIndex);
            // Gizmo operation selector
            if (ImGui::RadioButton("Translate", gizmoOp == GizmoOp::Translate)) gizmoOp = GizmoOp::Translate;
            ImGui::SameLine();
            if (ImGui::RadioButton("Rotate",    gizmoOp == GizmoOp::Rotate))    gizmoOp = GizmoOp::Rotate;
            ImGui::SameLine();
            if (ImGui::RadioButton("Scale",     gizmoOp == GizmoOp::Scale))     gizmoOp = GizmoOp::Scale;
            if (ImGui::RadioButton("World",     gizmoWorld))  gizmoWorld = true;
            ImGui::SameLine();
            if (ImGui::RadioButton("Local",     !gizmoWorld)) gizmoWorld = false;
        } else {
            ImGui::TextDisabled("No entity selected");
        }

        ImGui::End();

        // ── ImGuizmo entity manipulation ───────────────────────────────────
        if (selectedObjectIndex >= 0 && currentScene) {
            auto objs = currentScene->getGameObjects();
            if (selectedObjectIndex < static_cast<int>(objs.size())) {
                auto& obj = objs[selectedObjectIndex];

                float view[16], proj[16], objMatrix[16];
                memcpy(view, glm::value_ptr(currentView), sizeof(view));
                memcpy(proj, glm::value_ptr(currentProj), sizeof(proj));
                obj->GetTransformFloat16(objMatrix);

                ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
                if (gizmoOp == GizmoOp::Rotate) op = ImGuizmo::ROTATE;
                if (gizmoOp == GizmoOp::Scale)  op = ImGuizmo::SCALE;
                ImGuizmo::MODE mode = gizmoWorld ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

                if (ImGuizmo::Manipulate(view, proj, op, mode, objMatrix)) {
                    obj->SetTransformFromFloat16(objMatrix);
                }
            }
        }

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

        ImGui::SetCurrentContext(prevCtx);
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // 4. Submit command buffer
    VkSemaphore waitSemaphores[]   = {imageAvailableSemaphores[currentFrame]};
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = waitSemaphores;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(ctx->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to submit command buffer" << std::endl;
        return false;
    }

    // 5. Present
    VkSwapchainKHR swapchains[] = {ctx->getSwapchain()};

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = swapchains;
    presentInfo.pImageIndices      = &imageIndex;

    result = vkQueuePresentKHR(ctx->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return false; // Caller should recreate swapchain
    }

    // Advance to next frame slot
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}

// ─── Resize ──────────────────────────────────────────────────────────────────

bool VulkanRenderer::handleResize(uint32_t width, uint32_t height) {
    VkDevice device = ctx->getDevice();
    vkDeviceWaitIdle(device);

    cleanupFramebuffers();
    cleanupDepthResources();
    cleanupMSAAResources();
    cleanupIDBufferResources();  // Recreated lazily at new size
    cleanupGridResources();
    cleanupGrassResources();
    cleanupWaterResources();
    cleanupUIResources();

    // Destroy old per-swapchain-image semaphores before recreating swapchain
    for (auto& sem : renderFinishedSemaphores) {
        if (sem) vkDestroySemaphore(device, sem, nullptr);
    }
    renderFinishedSemaphores.clear();

    if (!ctx->recreateSwapchain(width, height)) return false;
    if (!createMSAAResources())                 return false;  // before depth + framebuffers
    if (!createDepthResources())                return false;
    if (!createFramebuffers())                  return false;

    // Recreate renderFinished semaphores for new swapchain image count
    uint32_t imageCount = static_cast<uint32_t>(ctx->getSwapchainImages().size());
    renderFinishedSemaphores.resize(imageCount);
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (uint32_t i = 0; i < imageCount; i++) {
        if (vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to recreate renderFinished semaphore" << std::endl;
            return false;
        }
    }

    if (!createGridResources())                 return false;
    if (!createUIResources())                   return false;
    if (modelUniformBuffers[0].buffer) {
        if (waterIndexCount == 0)       createWaterResources();
        if (grassBladeIndexCount == 0)  createGrassResources();
    }

    std::cout << "[Vulkan] Resized to " << width << "x" << height << std::endl;
    return true;
}

// ─── Quad Resources (Phase 4) ────────────────────────────────────────────────
//
// This sets up everything needed to draw a textured quad:
//   - VMA allocator for memory management
//   - Vertex buffer with 4 vertices (position + UV), uploaded via staging
//   - Index buffer with 6 indices (2 triangles), uploaded via staging
//   - Texture loaded from disk (VkImage + VkImageView + VkSampler)
//   - Uniform buffers for the MVP matrix (one per frame in flight)
//   - Descriptor set layout, pool, and sets to bind UBO + texture to shaders
//   - Graphics pipeline for textured rendering
//
// Index buffers let us reuse vertices. A quad has 4 unique corners but needs
// 6 vertices (2 triangles). Without an index buffer we'd duplicate 2 vertices.
// With complex meshes the savings are huge — a cube goes from 36 to 8 vertices.

// Must match the shader's vertex input and TexturedVertex in vk_pipeline.cpp
struct QuadVertex {
    glm::vec3 position;
    glm::vec2 texCoord;
};

bool VulkanRenderer::createTriangleResources() {
    VkDevice device = ctx->getDevice();

    // 1. Create VMA allocator
    allocator = createAllocator(ctx->getInstance(), ctx->getPhysicalDevice(), device);
    if (!allocator) return false;
    std::cout << "[Vulkan] VMA allocator created" << std::endl;

    // 2. Upload quad vertices via staging buffer
    //    A flat quad in the XY plane with UV coordinates
    QuadVertex vertices[] = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}}, // bottom-left
        {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}}, // bottom-right
        {{ 0.5f,  0.5f, 0.0f}, {1.0f, 0.0f}}, // top-right
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f}}, // top-left
    };

    vertexBuffer = createBufferWithStaging(
        allocator, device, commandPool, ctx->getGraphicsQueue(),
        vertices, sizeof(vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    if (!vertexBuffer.buffer) return false;
    std::cout << "[Vulkan] Quad vertex buffer uploaded" << std::endl;

    // 3. Upload index buffer via staging
    //    Two triangles: (0,1,2) and (0,2,3) = bottom-left → bottom-right →
    //    top-right, then bottom-left → top-right → top-left
    uint16_t indices[] = {0, 1, 2, 0, 2, 3};
    indexCount = 6;

    indexBuffer = createBufferWithStaging(
        allocator, device, commandPool, ctx->getGraphicsQueue(),
        indices, sizeof(indices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    if (!indexBuffer.buffer) return false;
    std::cout << "[Vulkan] Quad index buffer uploaded" << std::endl;

    // 4. Load texture from disk
    texture = loadTexture(
        allocator, device, ctx->getPhysicalDevice(),
        commandPool, ctx->getGraphicsQueue(),
        "assets/wooden_texture.png");

    if (!texture.image) {
        std::cerr << "[Vulkan] Failed to load texture, falling back to block.png" << std::endl;
        texture = loadTexture(
            allocator, device, ctx->getPhysicalDevice(),
            commandPool, ctx->getGraphicsQueue(),
            "assets/block.png");
    }

    if (!texture.image) return false;

    // 5. Create uniform buffers (CPU-visible, one per frame in flight)
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uniformBuffers[i] = createBuffer(
            allocator, sizeof(QuadMVPUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        if (!uniformBuffers[i].buffer) return false;
    }
    std::cout << "[Vulkan] Uniform buffers created" << std::endl;

    return true;
}

// ─── Descriptor Sets ─────────────────────────────────────────────────────────
//
// Descriptors are how Vulkan connects shader uniforms to actual GPU resources.
// In OpenGL you'd call glUniformMatrix4fv() — in Vulkan you:
//
//   1. Define a descriptor set LAYOUT (what type of resource, which binding)
//   2. Create a descriptor POOL (allocator for descriptor sets)
//   3. Allocate descriptor SETS from the pool
//   4. Update each set to point at the actual buffer/image
//   5. Bind the set during command buffer recording
//
// We need one descriptor set per frame in flight because each frame has its
// own uniform buffer with potentially different MVP data.

bool VulkanRenderer::createDescriptorSets() {
    VkDevice device = ctx->getDevice();

    // ── 1. Descriptor set layout ────────────────────────────────────────────
    //
    // Now we have TWO bindings:
    //   binding 0: UBO (MVP matrices) — used by vertex shader
    //   binding 1: Combined image sampler (texture) — used by fragment shader
    //
    // A "combined image sampler" bundles the VkImageView and VkSampler into
    // one descriptor. This is the most common way to pass textures in Vulkan
    // (equivalent to a sampler2D uniform in OpenGL/GLSL).

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    // Binding 0: UBO
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 1: Combined image sampler
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create descriptor set layout" << std::endl;
        return false;
    }

    // ── 2. Create the graphics pipeline (needs the descriptor set layout) ───
    if (!createTexturedPipeline(device, renderPass, descriptorSetLayout, pipelineLayout, pipeline, msaaSamples)) {
        return false;
    }

    // ── 3. Descriptor pool ──────────────────────────────────────────────────
    //
    // The pool must have enough capacity for ALL descriptor types we'll use.
    // We need MAX_FRAMES_IN_FLIGHT UBOs + MAX_FRAMES_IN_FLIGHT samplers.
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create descriptor pool" << std::endl;
        return false;
    }

    // ── 4. Allocate descriptor sets ─────────────────────────────────────────
    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
    layouts.fill(descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to allocate descriptor sets" << std::endl;
        return false;
    }

    // ── 5. Update each descriptor set with UBO + texture ────────────────────
    //
    // Each frame-in-flight gets its own descriptor set pointing to:
    //   - Its own uniform buffer (different MVP data per frame)
    //   - The SAME texture (textures don't change per frame here)
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // UBO descriptor
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(QuadMVPUniform);

        // Texture descriptor — combines the image view and sampler
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = texture.imageView;
        imageInfo.sampler     = texture.sampler;

        std::array<VkWriteDescriptorSet, 2> writes{};

        // Write 0: UBO at binding 0
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = descriptorSets[i];
        writes[0].dstBinding      = 0;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo     = &bufferInfo;

        // Write 1: Texture at binding 1
        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = descriptorSets[i];
        writes[1].dstBinding      = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }

    std::cout << "[Vulkan] Descriptor sets created (UBO + texture sampler)" << std::endl;
    return true;
}

// ─── Model Pipeline & Descriptors (Phase 5) ─────────────────────────────────
//
// Creates the model rendering pipeline and a 1x1 white fallback texture.
// The descriptor pool is sized dynamically based on the model's mesh count
// in loadModel(). This method sets up everything except per-mesh descriptors.

bool VulkanRenderer::createModelPipelineAndDescriptors() {
    VkDevice device = ctx->getDevice();

    // Descriptor set layout:
    //   binding 0 = FrameUBO (view, proj, lightSpaceMatrix, lightPos)
    //   binding 1 = diffuse texture sampler
    //   binding 2 = shadow map (comparison sampler for PCF)
    //   binding 3 = BoneUBO (bone matrices for skeletal animation)
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].binding            = 2;
    bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount    = 1;
    bindings[2].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].binding            = 3;
    bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount    = 1;
    bindings[3].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &modelDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create model descriptor set layout" << std::endl;
        return false;
    }

    // Create model pipeline
    if (!createModelPipeline(device, renderPass, modelDescriptorSetLayout,
                             modelPipelineLayout, modelPipeline, msaaSamples)) {
        return false;
    }

    // Create per-frame uniform buffers for model rendering (view + proj only)
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        modelUniformBuffers[i] = createBuffer(
            allocator, sizeof(FrameUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);
        if (!modelUniformBuffers[i].buffer) return false;

        boneUniformBuffers[i] = createBuffer(
            allocator, sizeof(BoneUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);
        if (!boneUniformBuffers[i].buffer) return false;
    }

    // Create a 1x1 white fallback texture for meshes without a diffuse texture.
    // This avoids needing a separate "no texture" pipeline — we just bind white.
    uint8_t whitePixel[] = {255, 255, 255, 255};

    // Create staging buffer with white pixel
    AllocatedBuffer staging = createBuffer(
        allocator, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    void* mapped;
    vmaMapMemory(allocator, staging.allocation, &mapped);
    memcpy(mapped, whitePixel, 4);
    vmaUnmapMemory(allocator, staging.allocation);

    // Create 1x1 VkImage
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {1, 1, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(allocator, &imageInfo, &allocCreateInfo,
                   &whiteTexture.image, &whiteTexture.allocation, nullptr);
    whiteTexture.width = 1;
    whiteTexture.height = 1;

    // Transition + copy + transition (reuse one-shot command pattern)
    // UNDEFINED → TRANSFER_DST
    {
        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmdAlloc{};
        cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAlloc.commandPool = commandPool;
        cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAlloc.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = whiteTexture.image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {1, 1, 1};
        vkCmdCopyBufferToImage(cmd, staging.buffer, whiteTexture.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(cmd);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        vkQueueSubmit(ctx->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(ctx->getGraphicsQueue());
        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    }
    destroyBuffer(allocator, staging);

    // Image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = whiteTexture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(device, &viewInfo, nullptr, &whiteTexture.imageView);

    // Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(device, &samplerInfo, nullptr, &whiteTexture.sampler);

    // Pre-allocate a large descriptor pool shared by all scene models.
    // 2000 sets is enough for hundreds of objects with multi-mesh models.
    if (!modelDescriptorPool) {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 4000;  // FrameUBO + BoneUBO across all sets
        poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 4000;  // diffuse + shadow across all sets

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();
        poolInfo.maxSets       = 2000;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &modelDescriptorPool) != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to create model descriptor pool" << std::endl;
            return false;
        }
    }

    std::cout << "[Vulkan] Model pipeline and white fallback texture created" << std::endl;
    return true;
}

// ─── PBR Pipeline and Descriptors (Phase 14) ─────────────────────────────────
//
// Creates the PBR pipeline, descriptor set layout, and fallback textures
// (flat normal map and black metallic/roughness).

bool VulkanRenderer::createPBRPipelineAndDescriptors() {
    VkDevice device = ctx->getDevice();

    // Descriptor layout:
    //   binding 0 = FrameUBO (vert+frag)
    //   binding 1 = albedo sampler (frag)
    //   binding 2 = normal map (frag)
    //   binding 3 = metallic map (frag)
    //   binding 4 = roughness map (frag)
    //   binding 5 = shadow map (frag)
    //   binding 6 = BoneUBO (vert)
    std::array<VkDescriptorSetLayoutBinding, 7> bindings{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    bindings[3] = {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    bindings[4] = {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    bindings[5] = {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    bindings[6] = {6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1, VK_SHADER_STAGE_VERTEX_BIT,   nullptr};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &pbrDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create PBR descriptor set layout" << std::endl;
        return false;
    }

    if (!createPBRPipeline(device, renderPass, pbrDescriptorSetLayout, pbrPipelineLayout, pbrPipeline, msaaSamples)) {
        return false;
    }

    // Helper lambda: create a 1x1 fallback texture from 4 bytes, with a given format.
    // Uses the same one-shot command buffer pattern as whiteTexture in createModelPipelineAndDescriptors.
    auto createFallbackTexture = [&](VulkanTexture& tex, const uint8_t* pixel, VkFormat format) -> bool {
        AllocatedBuffer staging = createBuffer(allocator, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        void* mapped;
        vmaMapMemory(allocator, staging.allocation, &mapped);
        memcpy(mapped, pixel, 4);
        vmaUnmapMemory(allocator, staging.allocation);

        VkImageCreateInfo imgInfo{};
        imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType     = VK_IMAGE_TYPE_2D;
        imgInfo.extent        = {1, 1, 1};
        imgInfo.mipLevels     = 1;
        imgInfo.arrayLayers   = 1;
        imgInfo.format        = format;
        imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vmaInfo{};
        vmaInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(allocator, &imgInfo, &vmaInfo, &tex.image, &tex.allocation, nullptr) != VK_SUCCESS) {
            destroyBuffer(allocator, staging);
            return false;
        }
        tex.width  = 1;
        tex.height = 1;

        // Transition + copy + transition (same pattern as whiteTexture)
        {
            VkCommandBuffer cmd;
            VkCommandBufferAllocateInfo cmdAlloc{};
            cmdAlloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmdAlloc.commandPool        = commandPool;
            cmdAlloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmdAlloc.commandBufferCount = 1;
            vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmd, &beginInfo);

            VkImageMemoryBarrier barrier{};
            barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image               = tex.image;
            barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            barrier.srcAccessMask       = 0;
            barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkBufferImageCopy region{};
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent      = {1, 1, 1};
            vkCmdCopyBufferToImage(cmd, staging.buffer, tex.image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            vkEndCommandBuffer(cmd);
            VkSubmitInfo submitInfo{};
            submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers    = &cmd;
            vkQueueSubmit(ctx->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(ctx->getGraphicsQueue());
            vkFreeCommandBuffers(device, commandPool, 1, &cmd);
        }
        destroyBuffer(allocator, staging);

        // Image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image            = tex.image;
        viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format           = format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCreateImageView(device, &viewInfo, nullptr, &tex.imageView);

        // Sampler
        VkSamplerCreateInfo sampInfo{};
        sampInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampInfo.magFilter    = VK_FILTER_LINEAR;
        sampInfo.minFilter    = VK_FILTER_LINEAR;
        sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        vkCreateSampler(device, &sampInfo, nullptr, &tex.sampler);

        return true;
    };

    // 1x1 flat normal texture: RGB = (128, 128, 255) = tangent-space up in UNORM
    {
        uint8_t flatNormal[] = {128, 128, 255, 255};
        if (!createFallbackTexture(flatNormalTexture, flatNormal, VK_FORMAT_R8G8B8A8_UNORM)) {
            std::cerr << "[Vulkan] Failed to create flat normal fallback texture" << std::endl;
            return false;
        }
    }

    // 1x1 black texture: for metallic/roughness fallback (value = 0.0)
    {
        uint8_t blackPixel[] = {0, 0, 0, 255};
        if (!createFallbackTexture(blackTexture, blackPixel, VK_FORMAT_R8G8B8A8_UNORM)) {
            std::cerr << "[Vulkan] Failed to create black fallback texture" << std::endl;
            return false;
        }
    }

    // Pre-allocate large PBR descriptor pool shared by all scene models
    if (!pbrDescriptorPool) {
        std::array<VkDescriptorPoolSize, 2> pbrPoolSizes{};
        pbrPoolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pbrPoolSizes[0].descriptorCount = 4000;  // FrameUBO + BoneUBO
        pbrPoolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pbrPoolSizes[1].descriptorCount = 10000; // albedo+normal+metallic+roughness+shadow

        VkDescriptorPoolCreateInfo pbrPoolInfo{};
        pbrPoolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pbrPoolInfo.poolSizeCount = static_cast<uint32_t>(pbrPoolSizes.size());
        pbrPoolInfo.pPoolSizes    = pbrPoolSizes.data();
        pbrPoolInfo.maxSets       = 2000;

        if (vkCreateDescriptorPool(device, &pbrPoolInfo, nullptr, &pbrDescriptorPool) != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to create PBR descriptor pool" << std::endl;
            return false;
        }
    }

    std::cout << "[Vulkan] PBR pipeline and fallback textures created" << std::endl;
    return true;
}

// ─── Shadow Resources (Phase 7) ──────────────────────────────────────────────
//
// Creates everything needed for the shadow pass:
//   - 2048x2048 depth image (D32_SFLOAT) for the shadow map
//   - Comparison sampler for hardware PCF (sampler2DShadow)
//   - Depth-only render pass + framebuffer
//   - Shadow pipeline + descriptor set (light UBO)
//   - Per-frame light UBOs

bool VulkanRenderer::createShadowResources() {
    VkDevice device = ctx->getDevice();

    // 1. Create shadow map depth image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(allocator, &imageInfo, &allocInfo,
                        &shadowImage, &shadowAllocation, nullptr) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create shadow map image" << std::endl;
        return false;
    }

    // 2. Image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = shadowImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &shadowImageView) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create shadow map image view" << std::endl;
        return false;
    }

    // 3. Comparison sampler — enables hardware PCF via sampler2DShadow
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor  = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // Outside shadow frustum = lit
    samplerInfo.compareEnable = VK_FALSE;  // MoltenVK doesn't support mutableComparisonSamplers

    if (vkCreateSampler(device, &samplerInfo, nullptr, &shadowSampler) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create shadow sampler" << std::endl;
        return false;
    }

    // 4. Depth-only render pass
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 0;
    subpass.pDepthStencilAttachment = &depthRef;

    // Dependency: shadow pass must finish before main pass samples the shadow map
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = 0;
    dependency.dstSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &depthAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &shadowRenderPass) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create shadow render pass" << std::endl;
        return false;
    }

    // 5. Framebuffer
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = shadowRenderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments    = &shadowImageView;
    fbInfo.width           = SHADOW_MAP_SIZE;
    fbInfo.height          = SHADOW_MAP_SIZE;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &shadowFramebuffer) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create shadow framebuffer" << std::endl;
        return false;
    }

    // 6. Shadow descriptor set layout (binding 0 = light UBO, binding 1 = bone UBO)
    std::array<VkDescriptorSetLayoutBinding, 2> shadowBindings{};
    shadowBindings[0].binding         = 0;
    shadowBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadowBindings[0].descriptorCount = 1;
    shadowBindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
    shadowBindings[1].binding         = 1;
    shadowBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadowBindings[1].descriptorCount = 1;
    shadowBindings[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo dsLayoutInfo{};
    dsLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsLayoutInfo.bindingCount = static_cast<uint32_t>(shadowBindings.size());
    dsLayoutInfo.pBindings    = shadowBindings.data();

    if (vkCreateDescriptorSetLayout(device, &dsLayoutInfo, nullptr, &shadowDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create shadow descriptor set layout" << std::endl;
        return false;
    }

    // 7. Shadow pipeline
    if (!createShadowPipeline(device, shadowRenderPass, shadowDescriptorSetLayout,
                               shadowPipelineLayout, shadowPipeline)) {
        return false;
    }

    // 8. Per-frame light UBOs
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        shadowUniformBuffers[i] = createBuffer(
            allocator, sizeof(LightUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);
        if (!shadowUniformBuffers[i].buffer) return false;
    }

    // 9. Descriptor pool + sets for shadow pass (light UBO + bone UBO per frame)
    VkDescriptorPoolSize shadowPoolSize{};
    shadowPoolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadowPoolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT * 2;  // light + bone per frame

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &shadowPoolSize;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &shadowDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create shadow descriptor pool" << std::endl;
        return false;
    }

    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
    layouts.fill(shadowDescriptorSetLayout);

    VkDescriptorSetAllocateInfo dsAllocInfo{};
    dsAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAllocInfo.descriptorPool     = shadowDescriptorPool;
    dsAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    dsAllocInfo.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(device, &dsAllocInfo, shadowDescriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to allocate shadow descriptor sets" << std::endl;
        return false;
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = shadowUniformBuffers[i].buffer;
        bufInfo.offset = 0;
        bufInfo.range  = sizeof(LightUBO);

        VkDescriptorBufferInfo boneBufInfo{};
        boneBufInfo.buffer = boneUniformBuffers[i].buffer;
        boneBufInfo.offset = 0;
        boneBufInfo.range  = sizeof(BoneUBO);

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = shadowDescriptorSets[i];
        writes[0].dstBinding      = 0;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo     = &bufInfo;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = shadowDescriptorSets[i];
        writes[1].dstBinding      = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo     = &boneBufInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }

    std::cout << "[Vulkan] Shadow resources created (" << SHADOW_MAP_SIZE << "x" << SHADOW_MAP_SIZE << ")" << std::endl;
    return true;
}

void VulkanRenderer::cleanupShadowResources() {
    VkDevice device = ctx->getDevice();
    if (!device) return;

    if (shadowPipeline)       { vkDestroyPipeline(device, shadowPipeline, nullptr); shadowPipeline = nullptr; }
    if (shadowPipelineLayout) { vkDestroyPipelineLayout(device, shadowPipelineLayout, nullptr); shadowPipelineLayout = nullptr; }
    if (shadowFramebuffer)    { vkDestroyFramebuffer(device, shadowFramebuffer, nullptr); shadowFramebuffer = nullptr; }
    if (shadowRenderPass)     { vkDestroyRenderPass(device, shadowRenderPass, nullptr); shadowRenderPass = nullptr; }
    if (shadowSampler)        { vkDestroySampler(device, shadowSampler, nullptr); shadowSampler = nullptr; }
    if (shadowImageView)      { vkDestroyImageView(device, shadowImageView, nullptr); shadowImageView = nullptr; }
    if (shadowImage && allocator) { vmaDestroyImage(allocator, shadowImage, shadowAllocation); shadowImage = nullptr; }
    if (shadowDescriptorPool) { vkDestroyDescriptorPool(device, shadowDescriptorPool, nullptr); shadowDescriptorPool = nullptr; }
    if (shadowDescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, shadowDescriptorSetLayout, nullptr); shadowDescriptorSetLayout = nullptr; }
    for (auto& buf : shadowUniformBuffers) {
        if (buf.buffer) destroyBuffer(allocator, buf);
    }
}

// ─── Skybox Resources (Phase 9) ──────────────────────────────────────────────

bool VulkanRenderer::createSkyboxResources() {
    VkDevice device = ctx->getDevice();

    // 1. Upload skybox cube vertices (36 verts, position only)
    float skyboxVertices[] = {
        // Back face
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        // Front face
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        // Left face
        -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
        // Right face
         1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,
        // Top face
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        // Bottom face
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
    };

    skyboxVertexBuffer = createBufferWithStaging(
        allocator, device, commandPool, ctx->getGraphicsQueue(),
        skyboxVertices, sizeof(skyboxVertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    if (!skyboxVertexBuffer.buffer) {
        std::cerr << "[Vulkan] Failed to create skybox vertex buffer" << std::endl;
        return false;
    }

    // 2. Load cubemap texture
    std::array<std::string, 6> faces = {
        "assets/skybox/right.jpg",
        "assets/skybox/left.jpg",
        "assets/skybox/top.jpg",
        "assets/skybox/bottom.jpg",
        "assets/skybox/front.jpg",
        "assets/skybox/back.jpg"
    };

    skyboxCubemap = loadCubemap(allocator, device, ctx->getPhysicalDevice(),
                                commandPool, ctx->getGraphicsQueue(), faces);
    if (skyboxCubemap.image == VK_NULL_HANDLE) {
        std::cerr << "[Vulkan] Failed to load skybox cubemap" << std::endl;
        return false;
    }

    // 3. Descriptor set layout: samplerCube at binding 0
    VkDescriptorSetLayoutBinding cubemapBinding{};
    cubemapBinding.binding         = 0;
    cubemapBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    cubemapBinding.descriptorCount = 1;
    cubemapBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &cubemapBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &skyboxDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create skybox descriptor set layout" << std::endl;
        return false;
    }

    // 4. Create pipeline
    if (!createSkyboxPipeline(device, renderPass, skyboxDescriptorSetLayout,
                              skyboxPipelineLayout, skyboxPipeline, msaaSamples)) {
        std::cerr << "[Vulkan] Failed to create skybox pipeline" << std::endl;
        return false;
    }

    // 5. Descriptor pool (1 set, 1 sampler)
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &skyboxDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create skybox descriptor pool" << std::endl;
        return false;
    }

    // 6. Allocate and update descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = skyboxDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &skyboxDescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &skyboxDescriptorSet) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to allocate skybox descriptor set" << std::endl;
        return false;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler     = skyboxCubemap.sampler;
    imageInfo.imageView   = skyboxCubemap.imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = skyboxDescriptorSet;
    write.dstBinding      = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    hasSkybox = true;
    std::cout << "[Vulkan] Skybox resources created" << std::endl;
    return true;
}

void VulkanRenderer::cleanupSkyboxResources() {
    VkDevice device = ctx->getDevice();
    if (!device) return;

    if (skyboxPipeline)       { vkDestroyPipeline(device, skyboxPipeline, nullptr); skyboxPipeline = nullptr; }
    if (skyboxPipelineLayout) { vkDestroyPipelineLayout(device, skyboxPipelineLayout, nullptr); skyboxPipelineLayout = nullptr; }
    if (skyboxDescriptorPool) { vkDestroyDescriptorPool(device, skyboxDescriptorPool, nullptr); skyboxDescriptorPool = nullptr; }
    if (skyboxDescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, skyboxDescriptorSetLayout, nullptr); skyboxDescriptorSetLayout = nullptr; }
    if (skyboxCubemap.image && allocator) { destroyTexture(allocator, device, skyboxCubemap); }
    if (skyboxVertexBuffer.buffer && allocator) { destroyBuffer(allocator, skyboxVertexBuffer); }
    hasSkybox = false;
}

// ─── ImGui Vulkan Integration (Phase 11) ─────────────────────────────────────

bool VulkanRenderer::initImGui() {
    VkDevice device = ctx->getDevice();

    // Create a separate ImGui context for the Vulkan window.
    // The OpenGL window has its own context — ImGui only allows one
    // renderer backend per context.
    ImGuiContext* prevCtx = ImGui::GetCurrentContext();
    imguiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(imguiContext);

    // Init GLFW backend for the Vulkan window (input handling)
    ImGui_ImplGlfw_InitForVulkan(ctx->getWindow(), true);

    // Init Vulkan renderer backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion       = VK_API_VERSION_1_0;
    initInfo.Instance         = ctx->getInstance();
    initInfo.PhysicalDevice   = ctx->getPhysicalDevice();
    initInfo.Device           = device;
    initInfo.QueueFamily      = ctx->getGraphicsQueueFamily();
    initInfo.Queue            = ctx->getGraphicsQueue();
    initInfo.DescriptorPool   = VK_NULL_HANDLE;  // let backend create one
    initInfo.DescriptorPoolSize = 1000;
    initInfo.RenderPass       = renderPass;
    initInfo.MinImageCount    = 2;
    initInfo.ImageCount       = static_cast<uint32_t>(ctx->getSwapchainImages().size());
    initInfo.MSAASamples      = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Subpass          = 0;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        std::cerr << "[Vulkan] Failed to init ImGui Vulkan backend" << std::endl;
        ImGui::SetCurrentContext(prevCtx);
        return false;
    }

    // Restore the OpenGL context as default
    ImGui::SetCurrentContext(prevCtx);

    imguiInitialized = true;
    std::cout << "[Vulkan] ImGui Vulkan backend initialized" << std::endl;
    return true;
}

void VulkanRenderer::cleanupImGui() {
    if (imguiInitialized) {
        ImGuiContext* prevCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiContext);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(imguiContext);
        imguiContext = nullptr;
        ImGui::SetCurrentContext(prevCtx);
        imguiInitialized = false;
    }
}

// ─── Grid Resources (Phase 12 — Infinite Grid) ───────────────────────────────
//
// Creates resources for the infinite grid overlay:
//   - Descriptor set layout: FrameUBO at binding 0
//   - Descriptor pool + sets (one per frame in flight)
//   - Grid pipeline (grid.vert + grid.frag, alpha blending, no vertex input)
//
// The grid is drawn as a fullscreen triangle; the fragment shader does
// ray-plane intersection against Y=0 and discards rays that miss or are
// too far away. Camera position is supplied via a 16-byte push constant.

bool VulkanRenderer::createGridResources() {
    VkDevice device = ctx->getDevice();

    // Descriptor set layout: just FrameUBO at binding 0 (fragment stage)
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding            = 0;
    uboBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount    = 1;
    uboBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &gridDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create grid descriptor set layout" << std::endl;
        return false;
    }

    if (!createGridPipeline(device, renderPass, gridDescriptorSetLayout,
                            gridPipelineLayout, gridPipeline, msaaSamples)) {
        std::cerr << "[Vulkan] Failed to create grid pipeline" << std::endl;
        return false;
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &gridDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create grid descriptor pool" << std::endl;
        return false;
    }

    // Allocate descriptor sets
    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
    layouts.fill(gridDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = gridDescriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(device, &allocInfo, gridDescriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to allocate grid descriptor sets" << std::endl;
        return false;
    }

    // Update descriptor sets if modelUniformBuffers are already created (may be called after loadModel).
    // If not yet created, updateGridDescriptorSets() will be called from loadModel().
    if (modelUniformBuffers[0].buffer) {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufInfo{};
            bufInfo.buffer = modelUniformBuffers[i].buffer;
            bufInfo.offset = 0;
            bufInfo.range  = sizeof(FrameUBO);

            VkWriteDescriptorSet write{};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet          = gridDescriptorSets[i];
            write.dstBinding      = 0;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo     = &bufInfo;

            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }
    }

    std::cout << "[Vulkan] Grid resources created" << std::endl;
    return true;
}

void VulkanRenderer::cleanupGridResources() {
    if (!ctx) return;
    VkDevice device = ctx->getDevice();

    if (gridPipeline)            { vkDestroyPipeline(device, gridPipeline, nullptr);                       gridPipeline = nullptr; }
    if (gridPipelineLayout)      { vkDestroyPipelineLayout(device, gridPipelineLayout, nullptr);           gridPipelineLayout = nullptr; }
    if (gridDescriptorPool)      { vkDestroyDescriptorPool(device, gridDescriptorPool, nullptr);           gridDescriptorPool = nullptr; }
    if (gridDescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, gridDescriptorSetLayout, nullptr); gridDescriptorSetLayout = nullptr; }
}

// ─── Grass Resources (Phase 17) ───────────────────────────────────────────────
//
// Generates a single grass blade mesh (8 quads tapered toward the tip) on the CPU,
// uploads it once, then renders up to MAX_GRASS_INSTANCES copies via instancing.
// No geometry shader needed — the blade is pre-expanded.

bool VulkanRenderer::createGrassResources() {
    VkDevice device = ctx->getDevice();
    VkQueue  queue  = ctx->getGraphicsQueue();

    // ── 1. Generate blade mesh ────────────────────────────────────────────────
    // Cross-shaped blade: two perpendicular flat quads so blades are visible
    // from all camera angles regardless of rotation. Each quad is SEGMENTS rows.
    // Vertex layout: vec3 pos, vec3 normal, vec2 uv
    {
        const int   SEGMENTS  = 4;
        const float HEIGHT    = 1.5f;   // taller blades
        const float WIDTH     = 0.25f;  // wide enough to see from a distance

        struct BladeVert { float x, y, z, nx, ny, nz, u, v; };
        std::vector<BladeVert> verts;
        std::vector<uint32_t>  indices;

        // Helper: add one flat quad strip (in XY or ZY plane) with given normal
        auto addQuad = [&](float dx, float dz, float nx, float nz) {
            uint32_t base = (uint32_t)verts.size();
            for (int seg = 0; seg <= SEGMENTS; ++seg) {
                float t = (float)seg / SEGMENTS;
                float y = t * HEIGHT;
                float w = WIDTH * (1.0f - t * 0.7f);  // taper toward tip
                verts.push_back({ -w * dx, y, -w * dz, nx, 0.0f, nz, 0.0f, t });
                verts.push_back({  w * dx, y,  w * dz, nx, 0.0f, nz, 1.0f, t });
            }
            for (int seg = 0; seg < SEGMENTS; ++seg) {
                uint32_t bl = base + seg * 2;
                indices.insert(indices.end(), { bl, bl+1, bl+2, bl+2, bl+1, bl+3 });
            }
        };

        addQuad(1.0f, 0.0f,  0.0f, 1.0f);  // XY plane quad (faces +Z)
        addQuad(0.0f, 1.0f,  1.0f, 0.0f);  // ZY plane quad (faces +X)

        grassBladeIndexCount = (uint32_t)indices.size();

        // Upload helper lambda (reuse from water pattern)
        auto upload = [&](void* data, VkDeviceSize size, VkBufferUsageFlagBits usage, AllocatedBuffer& dst) {
            VkBufferCreateInfo stagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT };
            VmaAllocationCreateInfo sai{}; sai.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            VkBuffer sb; VmaAllocation sa;
            vmaCreateBuffer(allocator, &stagingInfo, &sai, &sb, &sa, nullptr);
            void* mapped; vmaMapMemory(allocator, sa, &mapped); memcpy(mapped, data, size); vmaUnmapMemory(allocator, sa);

            VkBufferCreateInfo gpuInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage) };
            VmaAllocationCreateInfo gai{}; gai.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            vmaCreateBuffer(allocator, &gpuInfo, &gai, &dst.buffer, &dst.allocation, nullptr);

            VkCommandBufferAllocateInfo cbA{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
            VkCommandBuffer cb; vkAllocateCommandBuffers(device, &cbA, &cb);
            VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr };
            vkBeginCommandBuffer(cb, &bi);
            VkBufferCopy cp{ 0, 0, size }; vkCmdCopyBuffer(cb, sb, dst.buffer, 1, &cp);
            vkEndCommandBuffer(cb);
            VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cb, 0, nullptr };
            vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE); vkQueueWaitIdle(queue);
            vkFreeCommandBuffers(device, commandPool, 1, &cb);
            vmaDestroyBuffer(allocator, sb, sa);
        };

        upload(verts.data(),   verts.size()   * sizeof(BladeVert), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, grassBladeVertexBuffer);
        upload(indices.data(), indices.size()  * sizeof(uint32_t),  VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  grassBladeIndexBuffer);
    }

    // ── 2. Instance buffer (CPU-visible, populated with random placement) ────
    {
        // Matches pipeline binding 1 stride = 9 floats (36 bytes):
        //   vec3 pos(12) + float rot(4) + float scale(4) + float heightVar(4) + vec3 tint(12) = 36
        struct GI { float px, py, pz, rot, scale, hv, tr, tg, tb; };
        static_assert(sizeof(GI) == sizeof(float) * 9, "GI must be 36 bytes");

        VkDeviceSize bufSize = MAX_GRASS_INSTANCES * sizeof(GI);
        VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, bufSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT };
        VmaAllocationCreateInfo ai{}; ai.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; ai.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        vmaCreateBuffer(allocator, &bufInfo, &ai, &grassInstanceBuffer.buffer, &grassInstanceBuffer.allocation, nullptr);

        // Populate with random instances on a 60m×60m patch centered at origin
        void* mapped; vmaMapMemory(allocator, grassInstanceBuffer.allocation, &mapped);
        GI* instances = (GI*)mapped;

        srand(42);
        auto rnd = [&](float lo, float hi) { return lo + (float)rand() / RAND_MAX * (hi - lo); };

        grassInstanceCount = 0;
        for (uint32_t i = 0; i < MAX_GRASS_INSTANCES; ++i) {
            float x = rnd(-30.0f, 30.0f);
            float z = rnd(-30.0f, 30.0f);
            if (x * x + z * z < 9.0f) continue;  // exclusion under model
            instances[grassInstanceCount++] = {
                x, 0.01f, z,   // tiny Y offset avoids Z-fight with ground plane
                rnd(0.0f, 6.28f),
                rnd(0.8f, 1.5f),
                rnd(0.0f, 0.5f),
                rnd(0.25f, 0.55f), rnd(0.5f, 0.85f), rnd(0.15f, 0.35f)
            };
            if (grassInstanceCount >= MAX_GRASS_INSTANCES) break;
        }
        vmaUnmapMemory(allocator, grassInstanceBuffer.allocation);
    }

    // ── 3. Grass texture ─────────────────────────────────────────────────────
    {
        const char* texPath = "assets/grass.png";
        FILE* f = fopen(texPath, "rb");
        if (f) {
            fclose(f);
            grassTexture = loadTexture(allocator, device, ctx->getPhysicalDevice(),
                                       commandPool, queue, texPath);
        } else {
            // Fallback: 1×1 green pixel
            uint8_t px[4] = { 80, 140, 60, 255 };
            VkBufferCreateInfo si{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT };
            VmaAllocationCreateInfo sai{}; sai.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            VkBuffer sb; VmaAllocation sa;
            vmaCreateBuffer(allocator, &si, &sai, &sb, &sa, nullptr);
            void* mp; vmaMapMemory(allocator, sa, &mp); memcpy(mp, px, 4); vmaUnmapMemory(allocator, sa);

            VkImageCreateInfo ii{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            ii.imageType = VK_IMAGE_TYPE_2D; ii.extent = { 1, 1, 1 }; ii.mipLevels = 1; ii.arrayLayers = 1;
            ii.format = VK_FORMAT_R8G8B8A8_SRGB; ii.tiling = VK_IMAGE_TILING_OPTIMAL;
            ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            ii.samples = VK_SAMPLE_COUNT_1_BIT;
            VmaAllocationCreateInfo gai{}; gai.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            vmaCreateImage(allocator, &ii, &gai, &grassTexture.image, &grassTexture.allocation, nullptr);
            grassTexture.width = grassTexture.height = 1;

            VkCommandBufferAllocateInfo cba{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
            VkCommandBuffer cb; vkAllocateCommandBuffers(device, &cba, &cb);
            VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr };
            vkBeginCommandBuffer(cb, &bi);
            VkImageMemoryBarrier bar{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            bar.srcQueueFamilyIndex = bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bar.image = grassTexture.image; bar.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            bar.srcAccessMask = 0; bar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &bar);
            VkBufferImageCopy reg{}; reg.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }; reg.imageExtent = { 1, 1, 1 };
            vkCmdCopyBufferToImage(cb, sb, grassTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
            bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; bar.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &bar);
            vkEndCommandBuffer(cb);
            VkSubmitInfo subInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cb, 0, nullptr };
            vkQueueSubmit(queue, 1, &subInfo, VK_NULL_HANDLE); vkQueueWaitIdle(queue);
            vkFreeCommandBuffers(device, commandPool, 1, &cb);
            vmaDestroyBuffer(allocator, sb, sa);

            VkImageViewCreateInfo vi{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            vi.image = grassTexture.image; vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vi.format = VK_FORMAT_R8G8B8A8_SRGB; vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCreateImageView(device, &vi, nullptr, &grassTexture.imageView);
            VkSamplerCreateInfo sampI{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
            sampI.magFilter = sampI.minFilter = VK_FILTER_LINEAR;
            sampI.addressModeU = sampI.addressModeV = sampI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            vkCreateSampler(device, &sampI, nullptr, &grassTexture.sampler);
        }
    }

    // ── 4. Descriptor set layout: FrameUBO (0) + grass texture (1) ───────────
    {
        VkDescriptorSetLayoutBinding bindings[2] = {};
        bindings[0] = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1, VK_SHADER_STAGE_VERTEX_BIT,   nullptr };
        bindings[1] = { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

        VkDescriptorSetLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        li.bindingCount = 2; li.pBindings = bindings;
        vkCreateDescriptorSetLayout(device, &li, nullptr, &grassDescriptorSetLayout);

        VkDescriptorPoolSize sizes[2] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         MAX_FRAMES_IN_FLIGHT },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT }
        };
        VkDescriptorPoolCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.poolSizeCount = 2; pi.pPoolSizes = sizes; pi.maxSets = MAX_FRAMES_IN_FLIGHT;
        vkCreateDescriptorPool(device, &pi, nullptr, &grassDescriptorPool);

        std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
        layouts.fill(grassDescriptorSetLayout);
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = grassDescriptorPool; ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT; ai.pSetLayouts = layouts.data();
        vkAllocateDescriptorSets(device, &ai, grassDescriptorSets.data());

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorBufferInfo uboInfo{ modelUniformBuffers[i].buffer, 0, sizeof(FrameUBO) };
            VkDescriptorImageInfo  imgInfo{ grassTexture.sampler, grassTexture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet writes[2] = {};
            writes[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, grassDescriptorSets[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         nullptr, &uboInfo, nullptr };
            writes[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, grassDescriptorSets[i], 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imgInfo, nullptr, nullptr };
            vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
        }
    }

    // ── 5. Pipeline ──────────────────────────────────────────────────────────
    // Note: pipeline binding stride must match GI struct size (10 floats = 40 bytes).
    // createGrassPipeline uses stride=8 — we patch it by creating the pipeline ourselves.
    if (!createGrassPipeline(device, renderPass, grassDescriptorSetLayout,
                              grassPipelineLayout, grassPipeline, msaaSamples))
        return false;

    std::cout << "[Vulkan] Grass resources created ("
              << grassInstanceCount << " instances, "
              << grassBladeIndexCount << " blade indices)" << std::endl;
    return true;
}

void VulkanRenderer::cleanupGrassResources() {
    if (!ctx) return;
    VkDevice device = ctx->getDevice();

    if (grassPipeline)            { vkDestroyPipeline(device, grassPipeline, nullptr);             grassPipeline = nullptr; }
    if (grassPipelineLayout)      { vkDestroyPipelineLayout(device, grassPipelineLayout, nullptr); grassPipelineLayout = nullptr; }
    if (grassDescriptorPool)      { vkDestroyDescriptorPool(device, grassDescriptorPool, nullptr); grassDescriptorPool = nullptr; }
    if (grassDescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, grassDescriptorSetLayout, nullptr); grassDescriptorSetLayout = nullptr; }
    if (grassBladeVertexBuffer.buffer) { vmaDestroyBuffer(allocator, grassBladeVertexBuffer.buffer, grassBladeVertexBuffer.allocation); grassBladeVertexBuffer = {}; }
    if (grassBladeIndexBuffer.buffer)  { vmaDestroyBuffer(allocator, grassBladeIndexBuffer.buffer,  grassBladeIndexBuffer.allocation);  grassBladeIndexBuffer  = {}; }
    if (grassInstanceBuffer.buffer)    { vmaDestroyBuffer(allocator, grassInstanceBuffer.buffer,    grassInstanceBuffer.allocation);    grassInstanceBuffer    = {}; }
    grassBladeIndexCount = 0;
    grassInstanceCount   = 0;
    destroyTexture(allocator, device, grassTexture);
    showGrass = false;
}

// ─── Water Resources (Phase 16) ───────────────────────────────────────────────
//
// Creates:
//   - Flat XZ grid mesh (200×200 quads), uploaded as vertex/index buffers
//   - Reflection offscreen image (half swapchain size) + render pass + framebuffer
//   - Water descriptor set: FrameUBO (0), normal map (1), reflection texture (2)
//   - Water pipeline

bool VulkanRenderer::createWaterResources() {
    VkDevice device = ctx->getDevice();
    VkExtent2D extent = ctx->getSwapchainExtent();

    // ── 1. Generate water mesh (XZ grid centered at origin) ─────────────────
    {
        const int   GRID     = 100;       // quads per side
        const float SIZE     = 40.0f;     // half-size (total 80m×80m)
        const float STEP     = (SIZE * 2.0f) / GRID;

        struct WaterVertex { float x, y, z, u, v; };
        std::vector<WaterVertex> verts;
        std::vector<uint32_t>    indices;
        verts.reserve((GRID + 1) * (GRID + 1));
        indices.reserve(GRID * GRID * 6);

        for (int row = 0; row <= GRID; ++row) {
            for (int col = 0; col <= GRID; ++col) {
                float x = -SIZE + col * STEP;
                float z = -SIZE + row * STEP;
                float u = (float)col / GRID;
                float v = (float)row / GRID;
                verts.push_back({ x, 0.0f, z, u, v });
            }
        }

        for (int row = 0; row < GRID; ++row) {
            for (int col = 0; col < GRID; ++col) {
                uint32_t tl = row * (GRID + 1) + col;
                uint32_t tr = tl + 1;
                uint32_t bl = tl + (GRID + 1);
                uint32_t br = bl + 1;
                indices.insert(indices.end(), { tl, bl, tr, tr, bl, br });
            }
        }
        waterIndexCount = (uint32_t)indices.size();

        // Upload via staging buffers
        auto uploadBuffer = [&](void* data, VkDeviceSize size, VkBufferUsageFlagBits usage, AllocatedBuffer& dst) {
            VkBufferCreateInfo stagingInfo{};
            stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingInfo.size  = size;
            stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            VmaAllocationCreateInfo stagingAlloc{};
            stagingAlloc.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            VkBuffer stagingBuf; VmaAllocation stagingAllocation;
            vmaCreateBuffer(allocator, &stagingInfo, &stagingAlloc, &stagingBuf, &stagingAllocation, nullptr);
            void* mapped; vmaMapMemory(allocator, stagingAllocation, &mapped);
            memcpy(mapped, data, size); vmaUnmapMemory(allocator, stagingAllocation);

            VkBufferCreateInfo gpuInfo{};
            gpuInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            gpuInfo.size  = size;
            gpuInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;
            VmaAllocationCreateInfo gpuAlloc{};
            gpuAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            vmaCreateBuffer(allocator, &gpuInfo, &gpuAlloc, &dst.buffer, &dst.allocation, nullptr);

            VkCommandBufferAllocateInfo cbAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
            VkCommandBuffer cb;
            vkAllocateCommandBuffers(device, &cbAlloc, &cb);
            VkCommandBufferBeginInfo beg{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr };
            vkBeginCommandBuffer(cb, &beg);
            VkBufferCopy copy{ 0, 0, size };
            vkCmdCopyBuffer(cb, stagingBuf, dst.buffer, 1, &copy);
            vkEndCommandBuffer(cb);
            VkSubmitInfo sub{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cb, 0, nullptr };
            vkQueueSubmit(ctx->getGraphicsQueue(), 1, &sub, VK_NULL_HANDLE);
            vkQueueWaitIdle(ctx->getGraphicsQueue());
            vkFreeCommandBuffers(device, commandPool, 1, &cb);
            vmaDestroyBuffer(allocator, stagingBuf, stagingAllocation);
        };

        uploadBuffer(verts.data(),   verts.size()   * sizeof(WaterVertex),  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, waterVertexBuffer);
        uploadBuffer(indices.data(), indices.size()  * sizeof(uint32_t),     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  waterIndexBuffer);
    }

    // ── 2. Reflection offscreen image (half swapchain resolution) ────────────
    {
        uint32_t rW = std::max(1u, extent.width  / 2);
        uint32_t rH = std::max(1u, extent.height / 2);

        // Color image
        VkImageCreateInfo imgInfo{};
        imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType     = VK_IMAGE_TYPE_2D;
        imgInfo.extent        = { rW, rH, 1 };
        imgInfo.mipLevels     = 1;
        imgInfo.arrayLayers   = 1;
        imgInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
        imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        VmaAllocationCreateInfo gpuAlloc{}; gpuAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(allocator, &imgInfo, &gpuAlloc, &reflectionImage, &reflectionAllocation, nullptr);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image            = reflectionImage;
        viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format           = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCreateImageView(device, &viewInfo, nullptr, &reflectionImageView);

        VkSamplerCreateInfo sampInfo{};
        sampInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampInfo.magFilter    = VK_FILTER_LINEAR;
        sampInfo.minFilter    = VK_FILTER_LINEAR;
        sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device, &sampInfo, nullptr, &reflectionSampler);

        // Depth image
        VkImageCreateInfo depthInfo = imgInfo;
        depthInfo.format = depthFormat;
        depthInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        vmaCreateImage(allocator, &depthInfo, &gpuAlloc, &reflectionDepthImage, &reflectionDepthAlloc, nullptr);
        VkImageViewCreateInfo dvInfo = viewInfo;
        dvInfo.image  = reflectionDepthImage;
        dvInfo.format = depthFormat;
        dvInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vkCreateImageView(device, &dvInfo, nullptr, &reflectionDepthView);

        // Reflection render pass (color + depth, renders mirrored scene)
        VkAttachmentDescription colorAtt{};
        colorAtt.format         = VK_FORMAT_R8G8B8A8_SRGB;
        colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAtt.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription depthAtt{};
        depthAtt.format         = depthFormat;
        depthAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAtt.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkAttachmentDescription atts[2] = { colorAtt, depthAtt };
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 2;
        rpInfo.pAttachments    = atts;
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dep;
        vkCreateRenderPass(device, &rpInfo, nullptr, &reflectionRenderPass);

        VkImageView fbAtts[2] = { reflectionImageView, reflectionDepthView };
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = reflectionRenderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments    = fbAtts;
        fbInfo.width           = rW;
        fbInfo.height          = rH;
        fbInfo.layers          = 1;
        vkCreateFramebuffer(device, &fbInfo, nullptr, &reflectionFramebuffer);
    }

    // ── 3. Water normal map (try disk, fallback to flat normal) ──────────────
    {
        const char* normalPath = "assets/water_normal.png";
        FILE* f = fopen(normalPath, "rb");
        if (f) {
            fclose(f);
            waterNormalTexture = loadTexture(allocator, device, ctx->getPhysicalDevice(),
                                             commandPool, ctx->getGraphicsQueue(), normalPath);
        } else {
            // Create a 1×1 flat normal (0.5, 0.5, 1.0) in UNORM8
            uint8_t px[4] = { 128, 128, 255, 255 };
            VkBufferCreateInfo stagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT };
            VmaAllocationCreateInfo stagingAlloc{}; stagingAlloc.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            VkBuffer stagingBuf; VmaAllocation stagingAllocation;
            vmaCreateBuffer(allocator, &stagingInfo, &stagingAlloc, &stagingBuf, &stagingAllocation, nullptr);
            void* mapped; vmaMapMemory(allocator, stagingAllocation, &mapped);
            memcpy(mapped, px, 4); vmaUnmapMemory(allocator, stagingAllocation);

            VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            imgInfo.imageType = VK_IMAGE_TYPE_2D; imgInfo.extent = { 1, 1, 1 };
            imgInfo.mipLevels = 1; imgInfo.arrayLayers = 1;
            imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            VmaAllocationCreateInfo gpuAlloc{}; gpuAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            vmaCreateImage(allocator, &imgInfo, &gpuAlloc, &waterNormalTexture.image, &waterNormalTexture.allocation, nullptr);
            waterNormalTexture.width = waterNormalTexture.height = 1;

            VkCommandBufferAllocateInfo cbAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
            VkCommandBuffer cb; vkAllocateCommandBuffers(device, &cbAlloc, &cb);
            VkCommandBufferBeginInfo beg{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr };
            vkBeginCommandBuffer(cb, &beg);
            VkImageMemoryBarrier bar{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            bar.srcQueueFamilyIndex = bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bar.image = waterNormalTexture.image; bar.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            bar.srcAccessMask = 0; bar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &bar);
            VkBufferImageCopy reg{}; reg.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }; reg.imageExtent = { 1, 1, 1 };
            vkCmdCopyBufferToImage(cb, stagingBuf, waterNormalTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
            bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; bar.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &bar);
            vkEndCommandBuffer(cb);
            VkSubmitInfo sub{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cb, 0, nullptr };
            vkQueueSubmit(ctx->getGraphicsQueue(), 1, &sub, VK_NULL_HANDLE);
            vkQueueWaitIdle(ctx->getGraphicsQueue());
            vkFreeCommandBuffers(device, commandPool, 1, &cb);
            vmaDestroyBuffer(allocator, stagingBuf, stagingAllocation);

            VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            viewInfo.image = waterNormalTexture.image; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM; viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCreateImageView(device, &viewInfo, nullptr, &waterNormalTexture.imageView);

            VkSamplerCreateInfo sampInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
            sampInfo.magFilter = sampInfo.minFilter = VK_FILTER_LINEAR;
            sampInfo.addressModeU = sampInfo.addressModeV = sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            vkCreateSampler(device, &sampInfo, nullptr, &waterNormalTexture.sampler);
        }
    }

    // ── 4. Descriptor set layout: FrameUBO (0), normalMap (1), reflectionTex (2)
    {
        VkDescriptorSetLayoutBinding bindings[3] = {};
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[2].binding         = 2;
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 3;
        layoutInfo.pBindings    = bindings;
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &waterDescriptorSetLayout);

        VkDescriptorPoolSize poolSizes[2] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         MAX_FRAMES_IN_FLIGHT },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 2 }
        };
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes    = poolSizes;
        poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &waterDescriptorPool);

        std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
        layouts.fill(waterDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = waterDescriptorPool;
        allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        allocInfo.pSetLayouts        = layouts.data();
        vkAllocateDescriptorSets(device, &allocInfo, waterDescriptorSets.data());

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorBufferInfo uboInfo{};
            uboInfo.buffer = modelUniformBuffers[i].buffer;
            uboInfo.offset = 0;
            uboInfo.range  = sizeof(FrameUBO);

            VkDescriptorImageInfo normalInfo{};
            normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            normalInfo.imageView   = waterNormalTexture.imageView;
            normalInfo.sampler     = waterNormalTexture.sampler;

            VkDescriptorImageInfo reflInfo{};
            reflInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            reflInfo.imageView   = reflectionImageView;
            reflInfo.sampler     = reflectionSampler;

            VkWriteDescriptorSet writes[3] = {};
            writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet          = waterDescriptorSets[i];
            writes[0].dstBinding      = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].pBufferInfo     = &uboInfo;
            writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet          = waterDescriptorSets[i];
            writes[1].dstBinding      = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].pImageInfo      = &normalInfo;
            writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet          = waterDescriptorSets[i];
            writes[2].dstBinding      = 2;
            writes[2].descriptorCount = 1;
            writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].pImageInfo      = &reflInfo;
            vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
        }
    }

    // ── 5. Water pipeline ────────────────────────────────────────────────────
    if (!createWaterPipeline(device, renderPass, waterDescriptorSetLayout,
                              waterPipelineLayout, waterPipeline, msaaSamples))
        return false;

    std::cout << "[Vulkan] Water resources created (" << waterIndexCount << " indices)" << std::endl;
    return true;
}

void VulkanRenderer::cleanupWaterResources() {
    if (!ctx) return;
    VkDevice device = ctx->getDevice();

    if (waterPipeline)            { vkDestroyPipeline(device, waterPipeline, nullptr);             waterPipeline = nullptr; }
    if (waterPipelineLayout)      { vkDestroyPipelineLayout(device, waterPipelineLayout, nullptr); waterPipelineLayout = nullptr; }
    if (waterDescriptorPool)      { vkDestroyDescriptorPool(device, waterDescriptorPool, nullptr); waterDescriptorPool = nullptr; }
    if (waterDescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, waterDescriptorSetLayout, nullptr); waterDescriptorSetLayout = nullptr; }
    if (waterVertexBuffer.buffer) { vmaDestroyBuffer(allocator, waterVertexBuffer.buffer, waterVertexBuffer.allocation); waterVertexBuffer = {}; }
    if (waterIndexBuffer.buffer)  { vmaDestroyBuffer(allocator, waterIndexBuffer.buffer,  waterIndexBuffer.allocation);  waterIndexBuffer  = {}; }
    waterIndexCount = 0;

    // Reflection image
    if (reflectionFramebuffer) { vkDestroyFramebuffer(device, reflectionFramebuffer, nullptr);    reflectionFramebuffer = nullptr; }
    if (reflectionRenderPass)  { vkDestroyRenderPass(device, reflectionRenderPass, nullptr);       reflectionRenderPass  = nullptr; }
    if (reflectionDepthView)   { vkDestroyImageView(device, reflectionDepthView, nullptr);         reflectionDepthView   = nullptr; }
    if (reflectionDepthImage)  { vmaDestroyImage(allocator, reflectionDepthImage, reflectionDepthAlloc); reflectionDepthImage = nullptr; reflectionDepthAlloc = nullptr; }
    if (reflectionSampler)     { vkDestroySampler(device, reflectionSampler, nullptr);             reflectionSampler    = nullptr; }
    if (reflectionImageView)   { vkDestroyImageView(device, reflectionImageView, nullptr);         reflectionImageView   = nullptr; }
    if (reflectionImage)       { vmaDestroyImage(allocator, reflectionImage, reflectionAllocation); reflectionImage = nullptr; reflectionAllocation = nullptr; }

    destroyTexture(allocator, device, waterNormalTexture);
}

// ─── UI Text + Sprite Resources (Phase 15) ───────────────────────────────────
//
// Builds a glyph atlas texture from ASCII 32-126 using FreeType, then creates:
//   - R8_UNORM VkImage (atlas)                   — for text rendering
//   - Dynamic CPU-visible vertex buffer           — quad data rebuilt per renderText() call
//   - Shared descriptor set layout (1 sampler)   — used by both text and sprite pipelines
//   - uiTextPipeline / uiSpritePipeline
//
// Atlas layout: 16 columns × 6 rows, cell size = glyphCellW × glyphCellH.
// Each character occupies one cell; UVs are stored per-glyph in glyphs[].

bool VulkanRenderer::createUIResources() {
    VkDevice         device = ctx->getDevice();
    VkQueue          queue  = ctx->getGraphicsQueue();

    // ── 1. FreeType glyph atlas ───────────────────────────────────────────────
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "[UI] FreeType init failed" << std::endl;
        return false;
    }

    FT_Face face;
    const char* fontPath = "assets/fonts/BodoniXT.ttf";
    if (FT_New_Face(ft, fontPath, 0, &face)) {
        std::cerr << "[UI] Failed to load font: " << fontPath << std::endl;
        FT_Done_FreeType(ft);
        return false;
    }
    FT_Set_Pixel_Sizes(face, 0, 48);

    // Measure the largest glyph to set atlas cell size
    glyphCellW = 0;
    glyphCellH = 0;
    for (unsigned char c = 32; c < 127; ++c) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;
        glyphCellW = std::max(glyphCellW, (int)face->glyph->bitmap.width + 2);
        glyphCellH = std::max(glyphCellH, (int)face->glyph->bitmap.rows  + 2);
    }
    if (glyphCellW == 0) glyphCellW = 32;
    if (glyphCellH == 0) glyphCellH = 48;

    // Atlas: 16 columns × 6 rows = 96 chars (covers ASCII 32–127)
    const int COLS      = 16;
    const int ROWS      = 6;
    const int atlasW    = COLS * glyphCellW;
    const int atlasH    = ROWS * glyphCellH;

    // CPU buffer for atlas pixels (R8)
    std::vector<uint8_t> atlasPixels(atlasW * atlasH, 0);

    for (unsigned char c = 32; c < 127; ++c) {
        int idx = c - 32;
        int col = idx % COLS;
        int row = idx / COLS;

        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;

        FT_GlyphSlot g  = face->glyph;
        int bw          = (int)g->bitmap.width;
        int bh          = (int)g->bitmap.rows;
        int xOff        = col * glyphCellW;
        int yOff        = row * glyphCellH;

        // Blit glyph bitmap into atlas
        for (int y = 0; y < bh; ++y)
            for (int x = 0; x < bw; ++x)
                atlasPixels[(yOff + y) * atlasW + (xOff + x)] = g->bitmap.buffer[y * bw + x];

        // Store normalized UVs and metrics
        glyphs[c].u0       = (float)xOff        / atlasW;
        glyphs[c].v0       = (float)yOff        / atlasH;
        glyphs[c].u1       = (float)(xOff + bw) / atlasW;
        glyphs[c].v1       = (float)(yOff + bh) / atlasH;
        glyphs[c].bearingX = g->bitmap_left;
        glyphs[c].bearingY = g->bitmap_top;
        glyphs[c].advance  = (int)(g->advance.x >> 6);
        glyphs[c].width    = bw;
        glyphs[c].height   = bh;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // ── 2. Upload atlas to GPU (R8_UNORM VkImage via staging buffer) ─────────
    {
        VkDeviceSize size = atlasW * atlasH;

        // Staging buffer
        VkBufferCreateInfo stagingBufInfo{};
        stagingBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufInfo.size  = size;
        stagingBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VkBuffer      stagingBuf;
        VmaAllocation stagingAlloc;
        vmaCreateBuffer(allocator, &stagingBufInfo, &stagingAllocInfo, &stagingBuf, &stagingAlloc, nullptr);

        void* mapped;
        vmaMapMemory(allocator, stagingAlloc, &mapped);
        memcpy(mapped, atlasPixels.data(), size);
        vmaUnmapMemory(allocator, stagingAlloc);

        // Create R8_UNORM image
        VkImageCreateInfo imgInfo{};
        imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType     = VK_IMAGE_TYPE_2D;
        imgInfo.extent        = { (uint32_t)atlasW, (uint32_t)atlasH, 1 };
        imgInfo.mipLevels     = 1;
        imgInfo.arrayLayers   = 1;
        imgInfo.format        = VK_FORMAT_R8_UNORM;
        imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        VmaAllocationCreateInfo gpuAllocInfo{};
        gpuAllocInfo.usage    = VMA_MEMORY_USAGE_GPU_ONLY;

        vmaCreateImage(allocator, &imgInfo, &gpuAllocInfo,
                       &glyphAtlasTexture.image, &glyphAtlasTexture.allocation, nullptr);
        glyphAtlasTexture.width  = atlasW;
        glyphAtlasTexture.height = atlasH;

        // One-shot command buffer: transition → copy → transition
        VkCommandBufferAllocateInfo cbAlloc{};
        cbAlloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbAlloc.commandPool        = commandPool;
        cbAlloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAlloc.commandBufferCount = 1;
        VkCommandBuffer cb;
        vkAllocateCommandBuffers(device, &cbAlloc, &cb);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &beginInfo);

        // UNDEFINED → TRANSFER_DST
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = glyphAtlasTexture.image;
        barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask       = 0;
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent      = { (uint32_t)atlasW, (uint32_t)atlasH, 1 };
        vkCmdCopyBufferToImage(cb, stagingBuf, glyphAtlasTexture.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // TRANSFER_DST → SHADER_READ_ONLY
        barrier.oldLayout    = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(cb);
        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cb;
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        vkFreeCommandBuffers(device, commandPool, 1, &cb);

        vmaDestroyBuffer(allocator, stagingBuf, stagingAlloc);

        // Image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = glyphAtlasTexture.image;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = VK_FORMAT_R8_UNORM;
        viewInfo.subresourceRange                = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCreateImageView(device, &viewInfo, nullptr, &glyphAtlasTexture.imageView);

        // Sampler (linear, clamp)
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_LINEAR;
        samplerInfo.minFilter    = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.borderColor  = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        vkCreateSampler(device, &samplerInfo, nullptr, &glyphAtlasTexture.sampler);
    }

    // ── 3. Descriptor set layout: binding 0 = combined image sampler ─────────
    {
        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding         = 0;
        samplerBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &samplerBinding;
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &uiDescriptorSetLayout);

        // Pool: 2 sets (atlas + potential sprite), 2 samplers
        VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8 };
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.maxSets       = 8;
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &uiDescriptorPool);

        // Allocate descriptor set for the glyph atlas
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = uiDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &uiDescriptorSetLayout;
        vkAllocateDescriptorSets(device, &allocInfo, &glyphAtlasDescriptorSet);

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView   = glyphAtlasTexture.imageView;
        imgInfo.sampler     = glyphAtlasTexture.sampler;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = glyphAtlasDescriptorSet;
        write.dstBinding      = 0;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo      = &imgInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // ── 4. Pipelines ─────────────────────────────────────────────────────────
    if (!createUITextPipeline(device, renderPass, uiDescriptorSetLayout,
                              uiTextPipelineLayout, uiTextPipeline, msaaSamples))
        return false;

    if (!createUISpritePipeline(device, renderPass, uiDescriptorSetLayout,
                                uiSpritePipelineLayout, uiSpritePipeline, msaaSamples))
        return false;

    // ── 5. Dynamic vertex buffer (CPU-visible, updated each renderText call) ──
    {
        // 6 vertices per quad (2 triangles), 4 floats each (x,y,u,v)
        VkDeviceSize bufSize = UI_MAX_QUADS * 6 * sizeof(float) * 4;

        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size  = bufSize;
        bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        vmaCreateBuffer(allocator, &bufInfo, &allocInfo,
                        &uiVertexBuffer.buffer, &uiVertexBuffer.allocation, nullptr);
    }

    hasUIResources = true;
    std::cout << "[Vulkan] UI resources created (atlas " << COLS * glyphCellW << "x"
              << ROWS * glyphCellH << " px)" << std::endl;
    return true;
}

void VulkanRenderer::cleanupUIResources() {
    if (!ctx) return;
    VkDevice device = ctx->getDevice();

    if (uiVertexBuffer.buffer) {
        vmaDestroyBuffer(allocator, uiVertexBuffer.buffer, uiVertexBuffer.allocation);
        uiVertexBuffer = {};
    }
    if (uiTextPipeline)            { vkDestroyPipeline(device, uiTextPipeline, nullptr);             uiTextPipeline = nullptr; }
    if (uiTextPipelineLayout)      { vkDestroyPipelineLayout(device, uiTextPipelineLayout, nullptr); uiTextPipelineLayout = nullptr; }
    if (uiSpritePipeline)          { vkDestroyPipeline(device, uiSpritePipeline, nullptr);           uiSpritePipeline = nullptr; }
    if (uiSpritePipelineLayout)    { vkDestroyPipelineLayout(device, uiSpritePipelineLayout, nullptr); uiSpritePipelineLayout = nullptr; }
    if (uiDescriptorPool)          { vkDestroyDescriptorPool(device, uiDescriptorPool, nullptr);     uiDescriptorPool = nullptr; }
    if (uiDescriptorSetLayout)     { vkDestroyDescriptorSetLayout(device, uiDescriptorSetLayout, nullptr); uiDescriptorSetLayout = nullptr; }
    destroyTexture(allocator, device, glyphAtlasTexture);
    hasUIResources = false;
}

// ─── renderText ───────────────────────────────────────────────────────────────
// Builds a quad batch in uiVertexBuffer and issues a single draw call.
// Must be called inside an active render pass (main pass, after 3D content).
void VulkanRenderer::renderText(VkCommandBuffer cmd, const std::string& text,
                                float x, float y, float scale, glm::vec4 color)
{
    if (!hasUIResources || !uiTextPipeline) return;
    if (text.empty()) return;

    VkExtent2D extent = ctx->getSwapchainExtent();

    // Build quad vertices (x,y,u,v per vertex, 6 vertices per char)
    std::vector<float> verts;
    verts.reserve(text.size() * 6 * 4);

    float cursorX = x;
    uint32_t quadCount = 0;

    for (char c : text) {
        if ((unsigned char)c < 32 || (unsigned char)c > 126) { cursorX += glyphCellW * scale * 0.5f; continue; }
        const GlyphInfo& g = glyphs[(unsigned char)c];
        if (g.width == 0) { cursorX += g.advance * scale; continue; }

        float xPos = cursorX + g.bearingX * scale;
        float yPos = y       - g.bearingY * scale;  // top-left origin
        float w    = g.width  * scale;
        float h    = g.height * scale;

        // Two triangles forming a quad (top-left origin, Y down)
        float quad[6][4] = {
            { xPos,     yPos,     g.u0, g.v0 },
            { xPos,     yPos + h, g.u0, g.v1 },
            { xPos + w, yPos + h, g.u1, g.v1 },
            { xPos,     yPos,     g.u0, g.v0 },
            { xPos + w, yPos + h, g.u1, g.v1 },
            { xPos + w, yPos,     g.u1, g.v0 },
        };
        for (auto& v : quad)
            verts.insert(verts.end(), v, v + 4);

        cursorX += g.advance * scale;
        if (++quadCount >= UI_MAX_QUADS) break;
    }

    if (verts.empty()) return;

    // Upload to mapped vertex buffer
    void* mapped = nullptr;
    vmaMapMemory(allocator, uiVertexBuffer.allocation, &mapped);
    memcpy(mapped, verts.data(), verts.size() * sizeof(float));
    vmaUnmapMemory(allocator, uiVertexBuffer.allocation);

    // Orthographic projection for Vulkan: (0,0) top-left, (width,height) bottom-right.
    // glm::ortho(l,r,bottom,top): bottom→NDC -1, top→NDC +1.
    // In Vulkan NDC y=-1 is the top of the screen, so bottom=0, top=height maps
    // screen y=0 to NDC y=-1 (Vulkan top) and screen y=height to NDC y=+1 (bottom).
    glm::mat4 ortho = glm::ortho(0.0f, (float)extent.width,
                                  0.0f, (float)extent.height,
                                  -1.0f, 1.0f);

    // Push constant: mat4 ortho (64 bytes) + vec4 color (16 bytes)
    struct UIPush { glm::mat4 ortho; glm::vec4 color; } push{ ortho, color };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiTextPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            uiTextPipelineLayout, 0, 1, &glyphAtlasDescriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, uiTextPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(push), &push);

    VkBuffer     vbuf   = uiVertexBuffer.buffer;
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
    vkCmdDraw(cmd, quadCount * 6, 1, 0, 0);
}

// ─── ID Buffer Resources (Phase 11, Part 2 — Mouse Picking) ──────────────
//
// Creates an offscreen R32_UINT render target for object ID rendering.
// Each object is drawn with a unique integer ID via push constant.
// A single pixel is read back via a staging buffer on demand (mouse click).
//
// Resources:
//   - R32_UINT color image (same size as swapchain)
//   - D32_SFLOAT depth image (for correct depth testing)
//   - Render pass (color + depth attachments)
//   - Framebuffer
//   - Pipeline (id.vert + id.frag)
//   - Descriptor set: FrameUBO (binding 0) + BoneUBO (binding 1)
//   - Staging buffer for single-pixel readback (4 bytes)

bool VulkanRenderer::createIDBufferResources() {
    VkDevice device = ctx->getDevice();
    VkExtent2D extent = ctx->getSwapchainExtent();

    idBufferWidth  = extent.width;
    idBufferHeight = extent.height;

    // 1. R32_UINT color image
    VkImageCreateInfo colorImgInfo{};
    colorImgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorImgInfo.imageType     = VK_IMAGE_TYPE_2D;
    colorImgInfo.extent        = {extent.width, extent.height, 1};
    colorImgInfo.mipLevels     = 1;
    colorImgInfo.arrayLayers   = 1;
    colorImgInfo.format        = VK_FORMAT_R32_UINT;
    colorImgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    colorImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImgInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorImgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(allocator, &colorImgInfo, &allocInfo,
                        &idImage, &idAllocation, nullptr) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create ID buffer image" << std::endl;
        return false;
    }

    VkImageViewCreateInfo colorViewInfo{};
    colorViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorViewInfo.image                           = idImage;
    colorViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    colorViewInfo.format                          = VK_FORMAT_R32_UINT;
    colorViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.baseMipLevel   = 0;
    colorViewInfo.subresourceRange.levelCount     = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &colorViewInfo, nullptr, &idImageView) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create ID buffer image view" << std::endl;
        return false;
    }

    // 2. Depth image for the ID pass
    VkImageCreateInfo depthImgInfo{};
    depthImgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImgInfo.imageType     = VK_IMAGE_TYPE_2D;
    depthImgInfo.extent        = {extent.width, extent.height, 1};
    depthImgInfo.mipLevels     = 1;
    depthImgInfo.arrayLayers   = 1;
    depthImgInfo.format        = depthFormat;
    depthImgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    depthImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImgInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;

    if (vmaCreateImage(allocator, &depthImgInfo, &allocInfo,
                        &idDepthImage, &idDepthAllocation, nullptr) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create ID depth image" << std::endl;
        return false;
    }

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image                           = idDepthImage;
    depthViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format                          = depthFormat;
    depthViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel   = 0;
    depthViewInfo.subresourceRange.levelCount     = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &depthViewInfo, nullptr, &idDepthImageView) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create ID depth image view" << std::endl;
        return false;
    }

    // 3. Render pass: R32_UINT color + depth
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = VK_FORMAT_R32_UINT;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = depthFormat;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &idRenderPass) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create ID render pass" << std::endl;
        return false;
    }

    // 4. Framebuffer
    std::array<VkImageView, 2> fbAttachments = {idImageView, idDepthImageView};

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = idRenderPass;
    fbInfo.attachmentCount = static_cast<uint32_t>(fbAttachments.size());
    fbInfo.pAttachments    = fbAttachments.data();
    fbInfo.width           = extent.width;
    fbInfo.height          = extent.height;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &idFramebuffer) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create ID framebuffer" << std::endl;
        return false;
    }

    // 5. Descriptor set layout: FrameUBO (binding 0) + BoneUBO (binding 1)
    std::array<VkDescriptorSetLayoutBinding, 2> idBindings{};
    idBindings[0].binding         = 0;
    idBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    idBindings[0].descriptorCount = 1;
    idBindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
    idBindings[1].binding         = 1;
    idBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    idBindings[1].descriptorCount = 1;
    idBindings[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo dsLayoutInfo{};
    dsLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsLayoutInfo.bindingCount = static_cast<uint32_t>(idBindings.size());
    dsLayoutInfo.pBindings    = idBindings.data();

    if (vkCreateDescriptorSetLayout(device, &dsLayoutInfo, nullptr, &idDescriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create ID descriptor set layout" << std::endl;
        return false;
    }

    // 6. Pipeline
    if (!createIDPipeline(device, idRenderPass, idDescriptorSetLayout,
                           idPipelineLayout, idPipeline)) {
        return false;
    }

    // 7. Descriptor pool + sets
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT * 2;  // FrameUBO + BoneUBO per frame

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &idDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create ID descriptor pool" << std::endl;
        return false;
    }

    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
    layouts.fill(idDescriptorSetLayout);

    VkDescriptorSetAllocateInfo dsAllocInfo{};
    dsAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAllocInfo.descriptorPool     = idDescriptorPool;
    dsAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    dsAllocInfo.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(device, &dsAllocInfo, idDescriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to allocate ID descriptor sets" << std::endl;
        return false;
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo frameBuf{};
        frameBuf.buffer = modelUniformBuffers[i].buffer;
        frameBuf.offset = 0;
        frameBuf.range  = sizeof(FrameUBO);

        VkDescriptorBufferInfo boneBuf{};
        boneBuf.buffer = boneUniformBuffers[i].buffer;
        boneBuf.offset = 0;
        boneBuf.range  = sizeof(BoneUBO);

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = idDescriptorSets[i];
        writes[0].dstBinding      = 0;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo     = &frameBuf;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = idDescriptorSets[i];
        writes[1].dstBinding      = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo     = &boneBuf;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }

    // 8. Staging buffer for single-pixel readback (4 bytes = 1 uint32_t)
    idStagingBuffer = createBuffer(
        allocator, sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_TO_CPU);

    if (!idStagingBuffer.buffer) {
        std::cerr << "[Vulkan] Failed to create ID staging buffer" << std::endl;
        return false;
    }

    std::cout << "[Vulkan] ID buffer resources created (" << extent.width << "x" << extent.height << ")" << std::endl;
    return true;
}

void VulkanRenderer::cleanupIDBufferResources() {
    VkDevice device = ctx ? ctx->getDevice() : nullptr;
    if (!device) return;

    if (idDebugPipeline)       { vkDestroyPipeline(device, idDebugPipeline, nullptr); idDebugPipeline = nullptr; }
    if (idDebugPipelineLayout) { vkDestroyPipelineLayout(device, idDebugPipelineLayout, nullptr); idDebugPipelineLayout = nullptr; }
    if (idPipeline)       { vkDestroyPipeline(device, idPipeline, nullptr); idPipeline = nullptr; }
    if (idPipelineLayout) { vkDestroyPipelineLayout(device, idPipelineLayout, nullptr); idPipelineLayout = nullptr; }
    if (idFramebuffer)    { vkDestroyFramebuffer(device, idFramebuffer, nullptr); idFramebuffer = nullptr; }
    if (idRenderPass)     { vkDestroyRenderPass(device, idRenderPass, nullptr); idRenderPass = nullptr; }
    if (idImageView)      { vkDestroyImageView(device, idImageView, nullptr); idImageView = nullptr; }
    if (idImage && allocator) { vmaDestroyImage(allocator, idImage, idAllocation); idImage = nullptr; }
    if (idDepthImageView) { vkDestroyImageView(device, idDepthImageView, nullptr); idDepthImageView = nullptr; }
    if (idDepthImage && allocator) { vmaDestroyImage(allocator, idDepthImage, idDepthAllocation); idDepthImage = nullptr; }
    if (idDescriptorPool) { vkDestroyDescriptorPool(device, idDescriptorPool, nullptr); idDescriptorPool = nullptr; }
    if (idDescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, idDescriptorSetLayout, nullptr); idDescriptorSetLayout = nullptr; }
    if (idStagingBuffer.buffer && allocator) { destroyBuffer(allocator, idStagingBuffer); }
    idBufferWidth = 0;
    idBufferHeight = 0;
}

// ─── getObjectIdAtPixel — synchronous ID buffer render + readback ─────────
//
// Renders the entire scene to the R32_UINT ID buffer using a one-shot command
// buffer, then copies the requested pixel to a staging buffer for CPU readback.
// This is synchronous (blocks until GPU finishes) — call only on mouse click.
//
// Object IDs:
//   0 = background (clear value)
//   1 = ground plane
//   mesh index + 2 = model meshes (so mesh 0 = ID 2, mesh 1 = ID 3, etc.)

uint32_t VulkanRenderer::getObjectIdAtPixel(int x, int y) {
    if (sceneModels.empty() || !currentScene) return 0;

    // Lazy-create ID buffer resources on first use
    if (!idRenderPass) {
        if (!createIDBufferResources()) return 0;
    }

    VkDevice device = ctx->getDevice();
    VkExtent2D extent = ctx->getSwapchainExtent();

    // Recreate ID buffer if swapchain resized
    if (extent.width != idBufferWidth || extent.height != idBufferHeight) {
        vkDeviceWaitIdle(device);
        cleanupIDBufferResources();
        if (!createIDBufferResources()) return 0;
    }

    // Clamp coordinates
    if (x < 0 || y < 0 || static_cast<uint32_t>(x) >= extent.width || static_cast<uint32_t>(y) >= extent.height) {
        return 0;
    }

    // Update UBOs with current camera state (same as drawFrame)
    {
        FrameUBO frameUbo{};
        frameUbo.view = currentView;
        frameUbo.proj = currentProj;
        frameUbo.proj[1][1] *= -1; // Vulkan Y-flip

        void* mapped;
        vmaMapMemory(allocator, modelUniformBuffers[currentFrame].allocation, &mapped);
        memcpy(mapped, &frameUbo, sizeof(frameUbo));
        vmaUnmapMemory(allocator, modelUniformBuffers[currentFrame].allocation);
    }

    // Allocate one-shot command buffer
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool        = commandPool;
    cmdAlloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Begin ID render pass (clear to 0 = background)
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0, 0, 0, 0}};  // uint clear = 0
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpBeginInfo{};
    rpBeginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass        = idRenderPass;
    rpBeginInfo.framebuffer       = idFramebuffer;
    rpBeginInfo.renderArea.offset = {0, 0};
    rpBeginInfo.renderArea.extent = extent;
    rpBeginInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    rpBeginInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width    = static_cast<float>(extent.width);
    viewport.height   = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, idPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, idPipelineLayout,
                            0, 1, &idDescriptorSets[currentFrame], 0, nullptr);

    // Draw all scene objects (ID = scene object index + 2)
    uint32_t objIdx = 0;
    for (auto& obj : currentScene->getGameObjects()) {
        if (!obj->model) { objIdx++; continue; }
        auto it = sceneModels.find(obj->model.get());
        if (it == sceneModels.end()) { objIdx++; continue; }

        for (auto& meshGPU : it->second->meshes) {
            IDPushConstant push{};
            push.model    = obj->getModelMatrix();
            push.objectID = objIdx + 2;
            vkCmdPushConstants(cmd, idPipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(IDPushConstant), &push);

            VkBuffer vbufs[] = {meshGPU.buffers->vertexBuffer.buffer};
            VkDeviceSize voffs[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, voffs);
            vkCmdBindIndexBuffer(cmd, meshGPU.buffers->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, meshGPU.buffers->getIndexCount(), 1, 0, 0, 0);
        }
        objIdx++;
    }

    // Draw ground plane (ID = 1)
    if (groundVertexBuffer.buffer) {
        IDPushConstant groundPush{};
        groundPush.model    = glm::mat4(1.0f);
        groundPush.objectID = 1;
        vkCmdPushConstants(cmd, idPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(IDPushConstant), &groundPush);

        VkBuffer gvb[] = {groundVertexBuffer.buffer};
        VkDeviceSize gvo[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, gvb, gvo);
        vkCmdBindIndexBuffer(cmd, groundIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd);

    // Copy the single pixel at (x, y) from ID image → staging buffer
    // The render pass already transitions the image to TRANSFER_SRC_OPTIMAL
    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;  // tightly packed
    region.bufferImageHeight = 0;
    region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset       = {x, y, 0};
    region.imageExtent       = {1, 1, 1};

    vkCmdCopyImageToBuffer(cmd, idImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           idStagingBuffer.buffer, 1, &region);

    vkEndCommandBuffer(cmd);

    // Submit and wait synchronously
    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(ctx->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx->getGraphicsQueue());

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);

    // Read back the picked ID from the staging buffer
    uint32_t pickedID = 0;
    void* mapped;
    vmaMapMemory(allocator, idStagingBuffer.allocation, &mapped);
    memcpy(&pickedID, mapped, sizeof(uint32_t));
    vmaUnmapMemory(allocator, idStagingBuffer.allocation);

    lastPickedID = pickedID;
    // Update entity selection: IDs 2+ map to scene object indices (0-based)
    selectedObjectIndex = (pickedID >= 2) ? static_cast<int>(pickedID - 2) : -1;
    return pickedID;
}

// ─── Load Model ──────────────────────────────────────────────────────────────
//
// Takes a Model* (already loaded by Assimp with OpenGL textures) and creates
// Vulkan-side GPU resources for each mesh:
//   1. Upload vertex/index data to VkBuffers via VkMeshData (RHI)
//   2. Load diffuse textures as VulkanTexture (or use white fallback)
//   3. Create per-mesh descriptor sets pointing to UBO + texture
//
// This demonstrates the RHI in action: the Model's CPU-side vertex/index
// data (same std::vectors used by GLMeshData) gets uploaded to Vulkan
// through VkMeshData. Same data, different backend.

bool VulkanRenderer::loadModel(Model* model) {
    if (!model || !allocator) {
        std::cerr << "[Vulkan] loadModel: null model or allocator" << std::endl;
        return false;
    }
    // Skip if already uploaded
    if (sceneModels.count(model)) return true;

    VkDevice device = ctx->getDevice();

    std::cout << "[Vulkan] loadModel: " << model->meshes.size() << " meshes to upload" << std::endl;

    // Ensure pipelines and pools exist (no-ops if already created)
    if (!modelPipeline && !createModelPipelineAndDescriptors()) {
        std::cerr << "[Vulkan] loadModel: failed to create model pipeline" << std::endl;
        return false;
    }
    if (!pbrPipeline) createPBRPipelineAndDescriptors();  // non-fatal

    auto& newData = *(sceneModels[model] = std::make_unique<VulkanModelData>());
    uint32_t meshCount = static_cast<uint32_t>(model->meshes.size());

    // Per-model bone buffers (one per frame in flight)
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        newData.boneBuffers[i] = createBuffer(allocator, sizeof(BoneUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        if (!newData.boneBuffers[i].buffer) {
            std::cerr << "[Vulkan] Failed to create per-model bone buffer" << std::endl;
            sceneModels.erase(model);
            return false;
        }
    }

    newData.meshes.resize(meshCount);

    // Texture cache — avoid loading the same texture twice.
    std::unordered_map<std::string, VulkanTexture> textureCache;

    // Helper: load a texture by Mesh_Texture path, handling both disk files
    // and embedded GLB textures (path starts with '*').
    // srgb = true for albedo, false for normal/metallic/roughness.
    auto resolveTexture = [&](const std::string& texPath, bool srgb) -> VulkanTexture {
        auto it = textureCache.find(texPath);
        if (it != textureCache.end()) return it->second;

        VulkanTexture result{};

        if (!texPath.empty() && texPath[0] == '*') {
            // Embedded texture — index encoded as "*N"
            int idx = std::atoi(texPath.c_str() + 1);
            if (model->scene && idx >= 0 &&
                idx < static_cast<int>(model->scene->mNumTextures)) {
                const aiTexture* ait = model->scene->mTextures[idx];
                if (ait->mHeight == 0) {
                    // Compressed (PNG/JPG) — mWidth is byte count
                    result = loadTextureFromMemory(
                        allocator, device, ctx->getPhysicalDevice(),
                        commandPool, ctx->getGraphicsQueue(),
                        reinterpret_cast<const unsigned char*>(ait->pcData),
                        ait->mWidth, srgb);
                } else {
                    // Raw ARGB8888 — rare but handle it
                    // aiTexel is BGRA, stb_image not needed — upload directly
                    // Convert BGRA → RGBA on the fly
                    std::vector<uint8_t> rgba(ait->mWidth * ait->mHeight * 4);
                    for (uint32_t i = 0; i < ait->mWidth * ait->mHeight; i++) {
                        rgba[i*4+0] = ait->pcData[i].r;
                        rgba[i*4+1] = ait->pcData[i].g;
                        rgba[i*4+2] = ait->pcData[i].b;
                        rgba[i*4+3] = ait->pcData[i].a;
                    }
                    // Pack back as PNG bytes via stb — simplest path for raw texels
                    // For now treat as pre-decoded and upload directly
                    VkDeviceSize imgSize = rgba.size();
                    AllocatedBuffer staging = createBuffer(allocator, imgSize,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
                    void* mapped; vmaMapMemory(allocator, staging.allocation, &mapped);
                    memcpy(mapped, rgba.data(), imgSize); vmaUnmapMemory(allocator, staging.allocation);
                    // (reuse loadTextureFromMemory internals — just note this path is rare)
                    destroyBuffer(allocator, staging);
                    std::cerr << "[Vulkan] Raw aiTexture not yet supported, skipping" << std::endl;
                }
            }
        } else {
            // Disk file
            std::string fullPath = model->directory + "/" + texPath;
            result = loadTexture(allocator, device, ctx->getPhysicalDevice(),
                                 commandPool, ctx->getGraphicsQueue(), fullPath);
        }

        if (result.image) textureCache[texPath] = result;
        return result;
    };

    for (uint32_t m = 0; m < meshCount; m++) {
        Mesh& mesh = model->meshes[m];
        VulkanMeshGPUData& gpuMesh = newData.meshes[m];

        // 1. Upload mesh buffers via RHI (VkMeshData)
        gpuMesh.buffers = std::make_unique<VkMeshData>();
        std::cout << "[Vulkan] Mesh " << m << ": " << mesh.vertices.size()
                  << " verts, " << mesh.indices.size() << " indices" << std::endl;
        gpuMesh.buffers->setup(allocator, device, commandPool,
                               ctx->getGraphicsQueue(),
                               mesh.vertices, mesh.indices);

        // 2. Load textures — handles both disk paths and embedded '*N' paths
        bool hasDiffuse = false, hasNormal = false, hasMetallic = false, hasRoughness = false;

        for (const auto& tex : mesh.textures) {
            if (tex.type == "texture_diffuse" && !hasDiffuse) {
                gpuMesh.diffuseTexture = resolveTexture(tex.path, true);
                hasDiffuse = gpuMesh.diffuseTexture.image != VK_NULL_HANDLE;
            } else if (tex.type == "texture_normal" && !hasNormal) {
                gpuMesh.normalTexture = resolveTexture(tex.path, false);
                hasNormal = gpuMesh.normalTexture.image != VK_NULL_HANDLE;
            } else if (tex.type == "texture_metallic" && !hasMetallic) {
                gpuMesh.metallicTexture = resolveTexture(tex.path, false);
                hasMetallic = gpuMesh.metallicTexture.image != VK_NULL_HANDLE;
            } else if (tex.type == "texture_roughness" && !hasRoughness) {
                gpuMesh.roughnessTexture = resolveTexture(tex.path, false);
                hasRoughness = gpuMesh.roughnessTexture.image != VK_NULL_HANDLE;
            }
        }

        std::cout << "[Vulkan] Mesh " << m << " textures:"
                  << " diffuse=" << hasDiffuse
                  << " normal=" << hasNormal
                  << " metallic=" << hasMetallic
                  << " roughness=" << hasRoughness << std::endl;

        gpuMesh.hasPBR = pbrPipeline != nullptr && (hasDiffuse || hasNormal || hasMetallic || hasRoughness);

        // Determine which texture to use for descriptors
        VulkanTexture& texToUse = hasDiffuse ? gpuMesh.diffuseTexture : whiteTexture;

        // 3. Allocate and update descriptor sets for this mesh
        std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
        layouts.fill(modelDescriptorSetLayout);

        VkDescriptorSetAllocateInfo dsAllocInfo{};
        dsAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsAllocInfo.descriptorPool     = modelDescriptorPool;
        dsAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        dsAllocInfo.pSetLayouts        = layouts.data();

        VkResult dsResult = vkAllocateDescriptorSets(device, &dsAllocInfo, gpuMesh.descriptorSets.data());
        if (dsResult != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to allocate descriptor sets for mesh " << m
                      << " (result: " << dsResult << ")" << std::endl;
            return false;
        }
        std::cout << "[Vulkan] Mesh " << m << " descriptors allocated" << std::endl;

        for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
            VkDescriptorBufferInfo bufInfo{};
            bufInfo.buffer = modelUniformBuffers[f].buffer;
            bufInfo.offset = 0;
            bufInfo.range  = sizeof(FrameUBO);

            VkDescriptorImageInfo imgInfo{};
            imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgInfo.imageView   = texToUse.imageView;
            imgInfo.sampler     = texToUse.sampler;

            VkDescriptorImageInfo shadowImgInfo{};
            shadowImgInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            shadowImgInfo.imageView   = shadowImageView;
            shadowImgInfo.sampler     = shadowSampler;

            VkDescriptorBufferInfo boneInfo{};
            boneInfo.buffer = newData.boneBuffers[f].buffer;
            boneInfo.offset = 0;
            boneInfo.range  = sizeof(BoneUBO);

            std::array<VkWriteDescriptorSet, 4> writes{};
            writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet          = gpuMesh.descriptorSets[f];
            writes[0].dstBinding      = 0;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo     = &bufInfo;

            writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet          = gpuMesh.descriptorSets[f];
            writes[1].dstBinding      = 1;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo      = &imgInfo;

            writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet          = gpuMesh.descriptorSets[f];
            writes[2].dstBinding      = 2;
            writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].descriptorCount = 1;
            writes[2].pImageInfo      = &shadowImgInfo;

            writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet          = gpuMesh.descriptorSets[f];
            writes[3].dstBinding      = 3;
            writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[3].descriptorCount = 1;
            writes[3].pBufferInfo     = &boneInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                                   writes.data(), 0, nullptr);
        }

        // Allocate PBR descriptor sets if PBR pipeline is available
        if (gpuMesh.hasPBR && pbrDescriptorSetLayout && pbrDescriptorPool) {
            {
                std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> pbrLayouts;
                pbrLayouts.fill(pbrDescriptorSetLayout);

                VkDescriptorSetAllocateInfo pbrAllocInfo{};
                pbrAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                pbrAllocInfo.descriptorPool     = pbrDescriptorPool;
                pbrAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
                pbrAllocInfo.pSetLayouts        = pbrLayouts.data();

                if (vkAllocateDescriptorSets(device, &pbrAllocInfo, gpuMesh.pbrDescriptorSets.data()) != VK_SUCCESS) {
                    std::cerr << "[Vulkan] Failed to allocate PBR descriptor sets for mesh " << m << std::endl;
                    gpuMesh.hasPBR = false;
                } else {
                    // Pick actual textures or fallbacks
                    VulkanTexture& albedo    = hasDiffuse   ? gpuMesh.diffuseTexture   : whiteTexture;
                    VulkanTexture& normalTex = hasNormal    ? gpuMesh.normalTexture     : flatNormalTexture;
                    VulkanTexture& metalTex  = hasMetallic  ? gpuMesh.metallicTexture   : whiteTexture;
                    VulkanTexture& roughTex  = hasRoughness ? gpuMesh.roughnessTexture  : whiteTexture;

                    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
                        VkDescriptorBufferInfo frameInfo{};
                        frameInfo.buffer = modelUniformBuffers[f].buffer;
                        frameInfo.offset = 0;
                        frameInfo.range  = sizeof(FrameUBO);

                        VkDescriptorImageInfo albedoInfo{};
                        albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        albedoInfo.imageView   = albedo.imageView;
                        albedoInfo.sampler     = albedo.sampler;

                        VkDescriptorImageInfo normalInfo{};
                        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        normalInfo.imageView   = normalTex.imageView;
                        normalInfo.sampler     = normalTex.sampler;

                        VkDescriptorImageInfo metalInfo{};
                        metalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        metalInfo.imageView   = metalTex.imageView;
                        metalInfo.sampler     = metalTex.sampler;

                        VkDescriptorImageInfo roughInfo{};
                        roughInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        roughInfo.imageView   = roughTex.imageView;
                        roughInfo.sampler     = roughTex.sampler;

                        VkDescriptorImageInfo shadowInfo{};
                        shadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                        shadowInfo.imageView   = shadowImageView;
                        shadowInfo.sampler     = shadowSampler;

                        VkDescriptorBufferInfo boneInfo{};
                        boneInfo.buffer = newData.boneBuffers[f].buffer;
                        boneInfo.offset = 0;
                        boneInfo.range  = sizeof(BoneUBO);

                        std::array<VkWriteDescriptorSet, 7> pbrWrites{};
                        pbrWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        pbrWrites[0].dstSet          = gpuMesh.pbrDescriptorSets[f];
                        pbrWrites[0].dstBinding      = 0;
                        pbrWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                        pbrWrites[0].descriptorCount = 1;
                        pbrWrites[0].pBufferInfo     = &frameInfo;

                        pbrWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        pbrWrites[1].dstSet          = gpuMesh.pbrDescriptorSets[f];
                        pbrWrites[1].dstBinding      = 1;
                        pbrWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        pbrWrites[1].descriptorCount = 1;
                        pbrWrites[1].pImageInfo      = &albedoInfo;

                        pbrWrites[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        pbrWrites[2].dstSet          = gpuMesh.pbrDescriptorSets[f];
                        pbrWrites[2].dstBinding      = 2;
                        pbrWrites[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        pbrWrites[2].descriptorCount = 1;
                        pbrWrites[2].pImageInfo      = &normalInfo;

                        pbrWrites[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        pbrWrites[3].dstSet          = gpuMesh.pbrDescriptorSets[f];
                        pbrWrites[3].dstBinding      = 3;
                        pbrWrites[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        pbrWrites[3].descriptorCount = 1;
                        pbrWrites[3].pImageInfo      = &metalInfo;

                        pbrWrites[4].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        pbrWrites[4].dstSet          = gpuMesh.pbrDescriptorSets[f];
                        pbrWrites[4].dstBinding      = 4;
                        pbrWrites[4].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        pbrWrites[4].descriptorCount = 1;
                        pbrWrites[4].pImageInfo      = &roughInfo;

                        pbrWrites[5].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        pbrWrites[5].dstSet          = gpuMesh.pbrDescriptorSets[f];
                        pbrWrites[5].dstBinding      = 5;
                        pbrWrites[5].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        pbrWrites[5].descriptorCount = 1;
                        pbrWrites[5].pImageInfo      = &shadowInfo;

                        pbrWrites[6].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        pbrWrites[6].dstSet          = gpuMesh.pbrDescriptorSets[f];
                        pbrWrites[6].dstBinding      = 6;
                        pbrWrites[6].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                        pbrWrites[6].descriptorCount = 1;
                        pbrWrites[6].pBufferInfo     = &boneInfo;

                        vkUpdateDescriptorSets(device, static_cast<uint32_t>(pbrWrites.size()),
                                               pbrWrites.data(), 0, nullptr);
                    }
                }
            }
        }
    }

    std::cout << "[Vulkan] Model loaded: " << meshCount << " meshes"
              << (model->IsAnimated() ? " (animated)" : " (static)") << std::endl;
    return true;
}

// ─── Ground Plane ─────────────────────────────────────────────────────────────
// 20x20 quad at Y=0, acts as shadow receiver. Created once by loadScene().

bool VulkanRenderer::createGroundPlane() {
    if (groundVertexBuffer.buffer) return true;  // already created
    VkDevice device = ctx->getDevice();

    Vertex groundVerts[4];
    memset(groundVerts, 0, sizeof(groundVerts));
    groundVerts[0].Position = {-10.0f, 0.0f, -10.0f}; groundVerts[0].Normal = {0,1,0}; groundVerts[0].TexCoords = {0,0};
    groundVerts[1].Position = { 10.0f, 0.0f, -10.0f}; groundVerts[1].Normal = {0,1,0}; groundVerts[1].TexCoords = {1,0};
    groundVerts[2].Position = { 10.0f, 0.0f,  10.0f}; groundVerts[2].Normal = {0,1,0}; groundVerts[2].TexCoords = {1,1};
    groundVerts[3].Position = {-10.0f, 0.0f,  10.0f}; groundVerts[3].Normal = {0,1,0}; groundVerts[3].TexCoords = {0,1};
    uint32_t groundIndices[] = {0, 2, 1, 0, 3, 2};

    std::vector<Vertex> gv(groundVerts, groundVerts + 4);
    std::vector<unsigned int> gi(groundIndices, groundIndices + 6);

    groundVertexBuffer = createBufferWithStaging(allocator, device, commandPool, ctx->getGraphicsQueue(),
        gv.data(), gv.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    groundIndexBuffer  = createBufferWithStaging(allocator, device, commandPool, ctx->getGraphicsQueue(),
        gi.data(), gi.size() * sizeof(unsigned int), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> gndLayouts;
    gndLayouts.fill(modelDescriptorSetLayout);

    VkDescriptorSetAllocateInfo gndAllocInfo{};
    gndAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    gndAllocInfo.descriptorPool     = modelDescriptorPool;
    gndAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    gndAllocInfo.pSetLayouts        = gndLayouts.data();

    if (vkAllocateDescriptorSets(device, &gndAllocInfo, groundDescriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to allocate ground descriptor sets" << std::endl;
        return false;
    }

    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = modelUniformBuffers[f].buffer;
        bufInfo.offset = 0;
        bufInfo.range  = sizeof(FrameUBO);

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView   = whiteTexture.imageView;
        imgInfo.sampler     = whiteTexture.sampler;

        VkDescriptorImageInfo shadowImgInfo{};
        shadowImgInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowImgInfo.imageView   = shadowImageView;
        shadowImgInfo.sampler     = shadowSampler;

        VkDescriptorBufferInfo boneInfo{};
        boneInfo.buffer = boneUniformBuffers[f].buffer;  // identity — ground never animates
        boneInfo.offset = 0;
        boneInfo.range  = sizeof(BoneUBO);

        std::array<VkWriteDescriptorSet, 4> writes{};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, groundDescriptorSets[f], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         nullptr, &bufInfo,       nullptr};
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, groundDescriptorSets[f], 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imgInfo,       nullptr, nullptr};
        writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, groundDescriptorSets[f], 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowImgInfo, nullptr, nullptr};
        writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, groundDescriptorSets[f], 3, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         nullptr, &boneInfo,      nullptr};

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    std::cout << "[Vulkan] Ground plane created" << std::endl;
    return true;
}

// ─── Scene loading ────────────────────────────────────────────────────────────

bool VulkanRenderer::loadScene(Scene* scene) {
    if (!scene) return false;
    currentScene = scene;

    // Ensure model pipeline, pools, and UBOs exist
    if (!modelPipeline && !createModelPipelineAndDescriptors()) return false;
    if (!pbrPipeline) createPBRPipelineAndDescriptors();

    // Update grid descriptor sets now that modelUniformBuffers exist
    if (gridDescriptorPool && modelUniformBuffers[0].buffer) {
        VkDevice device = ctx->getDevice();
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufInfo{};
            bufInfo.buffer = modelUniformBuffers[i].buffer;
            bufInfo.offset = 0;
            bufInfo.range  = sizeof(FrameUBO);

            VkWriteDescriptorSet write{};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet          = gridDescriptorSets[i];
            write.dstBinding      = 0;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo     = &bufInfo;

            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }
    }

    if (!waterPipeline)  createWaterResources();
    if (!grassPipeline)  createGrassResources();

    // Ground plane (once)
    createGroundPlane();

    // Upload unique models
    for (auto& obj : scene->getGameObjects()) {
        if (obj->model && !sceneModels.count(obj->model.get()))
            loadModel(obj->model.get());
    }

    std::cout << "[Vulkan] Scene loaded: " << sceneModels.size()
              << " unique models, " << scene->getGameObjects().size() << " objects" << std::endl;
    return true;
}

// ─── Camera setters (Phase 6) ────────────────────────────────────────────────

void VulkanRenderer::setViewMatrix(const glm::mat4& view) {
    currentView = view;
}

void VulkanRenderer::setProjectionMatrix(const glm::mat4& proj) {
    currentProj = proj;
}

