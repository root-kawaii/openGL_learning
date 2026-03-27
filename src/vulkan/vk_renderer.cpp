#include <glad/glad.h>   // Must be before GLFW (pulled in by vk_context.h)
#include "vk_renderer.h"
#include "vk_pipeline.h"
#include "vk_texture.h"
#include "../model.h"
#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <iostream>
#include <limits>
#include <cstring>
#include <unordered_map>
#include <set>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

    if (!createRenderPass())      return false;
    if (!createDepthResources())  return false;
    if (!createFramebuffers())    return false;
    if (!createCommandPool())     return false;
    if (!createCommandBuffers())  return false;
    if (!createSyncObjects())        return false;
    if (!createTriangleResources())  return false;
    if (!createDescriptorSets())     return false;
    if (!createShadowResources())    return false;
    if (!createSkyboxResources())    return false;
    if (!initImGui())                return false;

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

    // Model resources (Phase 5)
    // Textures may be shared across meshes (cache), so deduplicate before destroying
    if (allocator && modelData) {
        std::set<VkImage> destroyedImages;
        for (auto& meshGPU : modelData->meshes) {
            if (meshGPU.diffuseTexture.image && destroyedImages.find(meshGPU.diffuseTexture.image) == destroyedImages.end()) {
                destroyedImages.insert(meshGPU.diffuseTexture.image);
                destroyTexture(allocator, device, meshGPU.diffuseTexture);
            }
            // VkMeshData destructor handles buffer cleanup via its unique_ptr
        }
        modelData.reset();
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

    // Color attachment — the swapchain image
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = ctx->getSwapchainFormat();
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;     // Clear at start
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;    // Keep result for present
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;       // Don't care about previous content
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Ready for presentation after pass

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = depthFormat;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE; // We don't need depth after pass
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass — references into the attachment array above
    VkAttachmentReference colorRef{};
    colorRef.attachment = 0; // Index into the attachment descriptions array
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // Subpass dependency — ensures the image is transitioned before we write to it.
    // Without this, the render pass might start before the image is acquired from
    // the swapchain, causing visual corruption.
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL; // Operations before the render pass
    dependency.dstSubpass    = 0;                    // Our subpass
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(ctx->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create render pass" << std::endl;
        return false;
    }

    std::cout << "[Vulkan] Render pass created" << std::endl;
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
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
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

// ─── Framebuffers ────────────────────────────────────────────────────────────
//
// A framebuffer binds actual images (VkImageView) to a render pass's attachment
// slots. We need one framebuffer per swapchain image because each has a
// different color image, but they all share the same depth image.

bool VulkanRenderer::createFramebuffers() {
    const auto& imageViews = ctx->getSwapchainImageViews();
    VkExtent2D extent = ctx->getSwapchainExtent();

    framebuffers.resize(imageViews.size());

    for (size_t i = 0; i < imageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            imageViews[i],  // color (attachment 0)
            depthImageView  // depth (attachment 1)
        };

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = renderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments    = attachments.data();
        fbInfo.width           = extent.width;
        fbInfo.height          = extent.height;
        fbInfo.layers          = 1;

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
    // Compute bone matrices on CPU, upload to bone UBO for this frame.
    // Must happen before both shadow and main passes.
    {
        BoneUBO boneUbo{};  // zero-initialized — non-animated meshes use totalWeight=0
        if (loadedModel && loadedModel->IsAnimated()) {
            std::vector<glm::mat4> transforms;
            loadedModel->GetBoneTransforms(transforms, static_cast<float>(glfwGetTime()));
            size_t count = std::min(transforms.size(), static_cast<size_t>(MAX_BONES));
            memcpy(boneUbo.bones, transforms.data(), count * sizeof(glm::mat4));
        }
        void* boneMapped;
        vmaMapMemory(allocator, boneUniformBuffers[currentFrame].allocation, &boneMapped);
        memcpy(boneMapped, &boneUbo, sizeof(boneUbo));
        vmaUnmapMemory(allocator, boneUniformBuffers[currentFrame].allocation);
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

    if (hasModel && modelData && shadowRenderPass) {
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
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipelineLayout,
                                0, 1, &shadowDescriptorSets[currentFrame], 0, nullptr);

        // Push model matrix and draw all meshes
        ModelPushConstant shadowPush{};
        shadowPush.model = currentModel;
        vkCmdPushConstants(cmd, shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(ModelPushConstant), &shadowPush);

        for (auto& meshGPU : modelData->meshes) {
            VkBuffer vbufs[] = {meshGPU.buffers->vertexBuffer.buffer};
            VkDeviceSize voffs[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, voffs);
            vkCmdBindIndexBuffer(cmd, meshGPU.buffers->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, meshGPU.buffers->getIndexCount(), 1, 0, 0, 0);
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

    if (hasModel && modelData) {
        // ── Model rendering path (Phase 6) ──────────────────────────────────
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipeline);

        // Update per-frame UBO (view, proj, light data)
        FrameUBO frameUbo{};
        frameUbo.view             = currentView;
        frameUbo.proj             = currentProj;
        frameUbo.proj[1][1]      *= -1; // Vulkan NDC Y-flip
        frameUbo.lightSpaceMatrix = lightSpaceMatrix;
        frameUbo.lightPos         = glm::vec4(lightPos, 1.0f);

        void* mapped;
        vmaMapMemory(allocator, modelUniformBuffers[currentFrame].allocation, &mapped);
        memcpy(mapped, &frameUbo, sizeof(frameUbo));
        vmaUnmapMemory(allocator, modelUniformBuffers[currentFrame].allocation);

        // Push per-object model matrix
        ModelPushConstant push{};
        push.model = currentModel;
        vkCmdPushConstants(cmd, modelPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(ModelPushConstant), &push);

        // Draw each mesh with its own descriptor set (different textures)
        for (auto& meshGPU : modelData->meshes) {
            VkBuffer vbuffers[] = {meshGPU.buffers->vertexBuffer.buffer};
            VkDeviceSize voffsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbuffers, voffsets);
            vkCmdBindIndexBuffer(cmd, meshGPU.buffers->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipelineLayout,
                                    0, 1, &meshGPU.descriptorSets[currentFrame], 0, nullptr);

            vkCmdDrawIndexed(cmd, meshGPU.buffers->getIndexCount(), 1, 0, 0, 0);
        }

        // Draw ground plane
        if (groundVertexBuffer.buffer) {
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

    // ── ID debug overlay (Phase 11, Part 2) ──────────────────────────────────
    // Re-renders all objects with semi-transparent false colors based on object ID
    if (showIDDebugOverlay && hasModel && modelData) {
        // Lazy-create the debug pipeline if needed
        if (!idDebugPipeline) {
            // Reuse the ID descriptor set layout (FrameUBO + BoneUBO)
            if (!idDescriptorSetLayout) {
                // Need to create ID resources first for the descriptor layout
                createIDBufferResources();
            }
            if (idDescriptorSetLayout) {
                createIDDebugPipeline(ctx->getDevice(), renderPass, idDescriptorSetLayout,
                                      idDebugPipelineLayout, idDebugPipeline);
            }
        }

        if (idDebugPipeline && idDescriptorSetLayout) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, idDebugPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, idDebugPipelineLayout,
                                    0, 1, &idDescriptorSets[currentFrame], 0, nullptr);

            // Draw model meshes (ID = mesh index + 2)
            for (uint32_t m = 0; m < static_cast<uint32_t>(modelData->meshes.size()); m++) {
                auto& meshGPU = modelData->meshes[m];

                IDPushConstant idPush{};
                idPush.model    = currentModel;
                idPush.objectID = m + 2;
                vkCmdPushConstants(cmd, idDebugPipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(IDPushConstant), &idPush);

                VkBuffer vbufs[] = {meshGPU.buffers->vertexBuffer.buffer};
                VkDeviceSize voffs[] = {0};
                vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, voffs);
                vkCmdBindIndexBuffer(cmd, meshGPU.buffers->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, meshGPU.buffers->getIndexCount(), 1, 0, 0, 0);
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

    // ── ImGui overlay (Phase 11) ────────────────────────────────────────────
    if (imguiInitialized) {
        ImGuiContext* prevCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiContext);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Vulkan Info");
        ImGui::Text("Renderer: Vulkan");
        ImGui::Text("Frame: %u", currentFrame);
        if (loadedModel) {
            ImGui::Text("Model: %s", loadedModel->IsAnimated() ? "Animated" : "Static");
        }
        ImGui::Separator();
        ImGui::Checkbox("Show ID Debug Overlay", &showIDDebugOverlay);
        ImGui::Text("ID Buffer: Click to pick");
        ImGui::Text("Last picked ID: %u", lastPickedID);
        if (lastPickedID == 0)      ImGui::Text("  -> Background");
        else if (lastPickedID == 1) ImGui::Text("  -> Ground plane");
        else                        ImGui::Text("  -> Mesh %u", lastPickedID - 2);
        ImGui::End();

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
    cleanupIDBufferResources();  // Recreated lazily at new size

    // Destroy old per-swapchain-image semaphores before recreating swapchain
    for (auto& sem : renderFinishedSemaphores) {
        if (sem) vkDestroySemaphore(device, sem, nullptr);
    }
    renderFinishedSemaphores.clear();

    if (!ctx->recreateSwapchain(width, height)) return false;
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
    if (!createTexturedPipeline(device, renderPass, descriptorSetLayout, pipelineLayout, pipeline)) {
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
                             modelPipelineLayout, modelPipeline)) {
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

    std::cout << "[Vulkan] Model pipeline and white fallback texture created" << std::endl;
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
                              skyboxPipelineLayout, skyboxPipeline)) {
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
    if (!hasModel || !modelData) return 0;

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

    // Draw model meshes (ID = mesh index + 2)
    for (uint32_t m = 0; m < static_cast<uint32_t>(modelData->meshes.size()); m++) {
        auto& meshGPU = modelData->meshes[m];

        IDPushConstant push{};
        push.model    = currentModel;
        push.objectID = m + 2;
        vkCmdPushConstants(cmd, idPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(IDPushConstant), &push);

        VkBuffer vbufs[] = {meshGPU.buffers->vertexBuffer.buffer};
        VkDeviceSize voffs[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, voffs);
        vkCmdBindIndexBuffer(cmd, meshGPU.buffers->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, meshGPU.buffers->getIndexCount(), 1, 0, 0, 0);
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
    VkDevice device = ctx->getDevice();

    std::cout << "[Vulkan] loadModel: " << model->meshes.size() << " meshes to upload" << std::endl;

    // Create model pipeline if not yet created
    if (!modelPipeline) {
        if (!createModelPipelineAndDescriptors()) {
            std::cerr << "[Vulkan] loadModel: failed to create model pipeline" << std::endl;
            return false;
        }
    }

    modelData = std::make_unique<VulkanModelData>();
    uint32_t meshCount = static_cast<uint32_t>(model->meshes.size());

    // Create descriptor pool sized for this model + ground plane
    // Each mesh needs MAX_FRAMES_IN_FLIGHT descriptor sets
    // Each set has: 2 UBOs (frame + bone) + 2 samplers (diffuse + shadow) = 4 descriptors
    // +MAX_FRAMES_IN_FLIGHT for the ground plane
    uint32_t totalSets = (meshCount + 1) * MAX_FRAMES_IN_FLIGHT;

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = totalSets * 2;  // FrameUBO + BoneUBO per set
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = totalSets * 2;  // diffuse + shadow per set

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = totalSets;

    if (modelDescriptorPool) {
        vkDestroyDescriptorPool(device, modelDescriptorPool, nullptr);
    }
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &modelDescriptorPool) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create model descriptor pool" << std::endl;
        return false;
    }

    modelData->meshes.resize(meshCount);

    // Texture cache — avoid loading the same file multiple times.
    // Shared textures are cleaned up once via the cache; per-mesh
    // diffuseTexture fields that reference cached textures are NOT
    // individually destroyed (see cleanup).
    std::unordered_map<std::string, VulkanTexture> textureCache;

    for (uint32_t m = 0; m < meshCount; m++) {
        Mesh& mesh = model->meshes[m];
        VulkanMeshGPUData& gpuMesh = modelData->meshes[m];

        // 1. Upload mesh buffers via RHI (VkMeshData)
        gpuMesh.buffers = std::make_unique<VkMeshData>();
        std::cout << "[Vulkan] Mesh " << m << ": " << mesh.vertices.size()
                  << " verts, " << mesh.indices.size() << " indices" << std::endl;
        gpuMesh.buffers->setup(allocator, device, commandPool,
                               ctx->getGraphicsQueue(),
                               mesh.vertices, mesh.indices);

        // 2. Load diffuse texture (cached by path to avoid duplicate uploads)
        bool hasDiffuse = false;
        for (const auto& tex : mesh.textures) {
            if (tex.type == "texture_diffuse") {
                std::string texPath = model->directory + "/" + tex.path;
                auto it = textureCache.find(texPath);
                if (it != textureCache.end()) {
                    gpuMesh.diffuseTexture = it->second;
                    hasDiffuse = true;
                } else {
                    gpuMesh.diffuseTexture = loadTexture(
                        allocator, device, ctx->getPhysicalDevice(),
                        commandPool, ctx->getGraphicsQueue(), texPath);
                    if (gpuMesh.diffuseTexture.image) {
                        textureCache[texPath] = gpuMesh.diffuseTexture;
                        hasDiffuse = true;
                    }
                }
                break;
            }
        }

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
            boneInfo.buffer = boneUniformBuffers[f].buffer;
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
    }

    // ── Ground plane (shadow receiver) ────────────────────────────────────
    {
        // 20x20 quad at Y=0
        Vertex groundVerts[4];
        memset(groundVerts, 0, sizeof(groundVerts));
        groundVerts[0].Position = {-10.0f, 0.0f, -10.0f}; groundVerts[0].Normal = {0,1,0}; groundVerts[0].TexCoords = {0,0};
        groundVerts[1].Position = { 10.0f, 0.0f, -10.0f}; groundVerts[1].Normal = {0,1,0}; groundVerts[1].TexCoords = {1,0};
        groundVerts[2].Position = { 10.0f, 0.0f,  10.0f}; groundVerts[2].Normal = {0,1,0}; groundVerts[2].TexCoords = {1,1};
        groundVerts[3].Position = {-10.0f, 0.0f,  10.0f}; groundVerts[3].Normal = {0,1,0}; groundVerts[3].TexCoords = {0,1};

        uint32_t groundIndices[] = {0, 2, 1, 0, 3, 2};

        std::vector<Vertex> gv(groundVerts, groundVerts + 4);
        std::vector<unsigned int> gi(groundIndices, groundIndices + 6);

        // Upload via staging
        VkDeviceSize vSize = gv.size() * sizeof(Vertex);
        VkDeviceSize iSize = gi.size() * sizeof(unsigned int);

        groundVertexBuffer = createBufferWithStaging(
            allocator, device, commandPool, ctx->getGraphicsQueue(),
            gv.data(), vSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        groundIndexBuffer = createBufferWithStaging(
            allocator, device, commandPool, ctx->getGraphicsQueue(),
            gi.data(), iSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

        // Allocate ground descriptor sets
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
            boneInfo.buffer = boneUniformBuffers[f].buffer;
            boneInfo.offset = 0;
            boneInfo.range  = sizeof(BoneUBO);

            std::array<VkWriteDescriptorSet, 4> writes{};
            writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet          = groundDescriptorSets[f];
            writes[0].dstBinding      = 0;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo     = &bufInfo;

            writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet          = groundDescriptorSets[f];
            writes[1].dstBinding      = 1;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo      = &imgInfo;

            writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet          = groundDescriptorSets[f];
            writes[2].dstBinding      = 2;
            writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].descriptorCount = 1;
            writes[2].pImageInfo      = &shadowImgInfo;

            writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet          = groundDescriptorSets[f];
            writes[3].dstBinding      = 3;
            writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[3].descriptorCount = 1;
            writes[3].pBufferInfo     = &boneInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                                   writes.data(), 0, nullptr);
        }
        std::cout << "[Vulkan] Ground plane created" << std::endl;
    }

    loadedModel = model;
    hasModel = true;
    std::cout << "[Vulkan] Model loaded: " << meshCount << " meshes"
              << (model->IsAnimated() ? " (animated)" : " (static)") << std::endl;
    return true;
}

// ─── Camera setters (Phase 6) ────────────────────────────────────────────────

void VulkanRenderer::setViewMatrix(const glm::mat4& view) {
    currentView = view;
}

void VulkanRenderer::setProjectionMatrix(const glm::mat4& proj) {
    currentProj = proj;
}

void VulkanRenderer::setModelTransform(const glm::mat4& model) {
    currentModel = model;
}
