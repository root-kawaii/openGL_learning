#include "vk_renderer.h"
#include "vk_pipeline.h"
#include <iostream>
#include <limits>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

    std::cout << "[Vulkan] Renderer initialized" << std::endl;
    return true;
}

void VulkanRenderer::cleanup() {
    if (!ctx) return;
    VkDevice device = ctx->getDevice();
    if (!device) return;

    vkDeviceWaitIdle(device);

    // Sync objects
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (renderFinishedSemaphores[i]) vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        if (inFlightFences[i])           vkDestroyFence(device, inFlightFences[i], nullptr);
        renderFinishedSemaphores[i] = nullptr;
        imageAvailableSemaphores[i] = nullptr;
        inFlightFences[i] = nullptr;
    }

    // Triangle resources
    if (allocator) {
        destroyBuffer(allocator, vertexBuffer);
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

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to create sync objects for frame " << i << std::endl;
            return false;
        }
    }

    std::cout << "[Vulkan] Sync objects created (" << MAX_FRAMES_IN_FLIGHT
              << " frames in flight)" << std::endl;
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

    // ── Draw the triangle ────────────────────────────────────────────────
    // Bind pipeline, set dynamic viewport/scissor, bind vertex buffer,
    // bind descriptor set (UBO), and draw 3 vertices.

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Dynamic viewport and scissor (so we don't need to recreate pipeline on resize)
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

    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {vertexBuffer.buffer};
    VkDeviceSize offsets[]   = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

    // Update and bind the UBO for this frame
    {
        float time = static_cast<float>(glfwGetTime());
        float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);

        MVPUniform ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj  = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1; // Vulkan Y is flipped vs OpenGL

        void* mapped;
        vmaMapMemory(allocator, uniformBuffers[currentFrame].allocation, &mapped);
        memcpy(mapped, &ubo, sizeof(ubo));
        vmaUnmapMemory(allocator, uniformBuffers[currentFrame].allocation);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                            0, 1, &descriptorSets[currentFrame], 0, nullptr);

    vkCmdDraw(cmd, 3, 1, 0, 0); // 3 vertices, 1 instance

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // 4. Submit command buffer
    VkSemaphore waitSemaphores[]   = {imageAvailableSemaphores[currentFrame]};
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
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
    vkDeviceWaitIdle(ctx->getDevice());

    cleanupFramebuffers();
    cleanupDepthResources();

    if (!ctx->recreateSwapchain(width, height)) return false;
    if (!createDepthResources())                return false;
    if (!createFramebuffers())                  return false;

    std::cout << "[Vulkan] Resized to " << width << "x" << height << std::endl;
    return true;
}

// ─── Triangle Resources ──────────────────────────────────────────────────────
//
// This sets up everything needed to draw a colored triangle:
//   - VMA allocator for memory management
//   - Vertex buffer with 3 colored vertices (uploaded via staging)
//   - Uniform buffers for the MVP matrix (one per frame in flight)
//   - Descriptor set layout, pool, and sets to bind the UBO to the shader
//   - Graphics pipeline

// Same struct as in vk_pipeline.cpp — must match the shader's vertex input
struct TriangleVertex {
    glm::vec3 position;
    glm::vec3 color;
};

bool VulkanRenderer::createTriangleResources() {
    VkDevice device = ctx->getDevice();

    // 1. Create VMA allocator
    allocator = createAllocator(ctx->getInstance(), ctx->getPhysicalDevice(), device);
    if (!allocator) return false;
    std::cout << "[Vulkan] VMA allocator created" << std::endl;

    // 2. Upload triangle vertices via staging buffer
    //    RGB triangle at the origin
    TriangleVertex vertices[] = {
        {{ 0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // bottom — red
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // right  — green
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // left   — blue
    };

    vertexBuffer = createBufferWithStaging(
        allocator, device, commandPool, ctx->getGraphicsQueue(),
        vertices, sizeof(vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    if (!vertexBuffer.buffer) return false;
    std::cout << "[Vulkan] Triangle vertex buffer uploaded" << std::endl;

    // 3. Create uniform buffers (CPU-visible, one per frame in flight)
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uniformBuffers[i] = createBuffer(
            allocator, sizeof(MVPUniform),
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

    // 1. Descriptor set layout — one UBO at binding 0, visible in vertex shader
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding            = 0;
    uboBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount    = 1;
    uboBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create descriptor set layout" << std::endl;
        return false;
    }

    // 2. Create the graphics pipeline (needs the descriptor set layout)
    if (!createTrianglePipeline(device, renderPass, descriptorSetLayout, pipelineLayout, pipeline)) {
        return false;
    }

    // 3. Descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create descriptor pool" << std::endl;
        return false;
    }

    // 4. Allocate descriptor sets
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

    // 5. Point each descriptor set at its uniform buffer
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(MVPUniform);

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = descriptorSets[i];
        write.dstBinding      = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    std::cout << "[Vulkan] Descriptor sets created and bound" << std::endl;
    return true;
}
