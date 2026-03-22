#pragma once

#include "vk_context.h"
#include "vk_buffer.h"
#include <vector>
#include <array>

#include <glm/glm.hpp>

// Maximum number of frames that can be rendered concurrently.
// While the GPU works on frame N, we prepare frame N+1 on the CPU.
// This is called "frames in flight" — it prevents CPU/GPU idle time.
static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// UBO matching the shader's UniformBufferObject
struct MVPUniform {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // Initialize renderer with an already-initialized VulkanContext
    bool init(VulkanContext* context);

    // Draw a frame — renders the triangle with updated MVP
    bool drawFrame();

    // Handle window resize — recreates swapchain + framebuffers
    bool handleResize(uint32_t width, uint32_t height);

    // Cleanup
    void cleanup();

    // Set clear color (RGBA, 0.0-1.0)
    void setClearColor(float r, float g, float b, float a = 1.0f);

private:
    bool createRenderPass();
    bool createDepthResources();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createTriangleResources();
    bool createDescriptorSets();

    void cleanupDepthResources();
    void cleanupFramebuffers();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findDepthFormat();

    VulkanContext* ctx = nullptr;

    // Render pass
    VkRenderPass renderPass = nullptr;

    // Depth buffer
    VkImage        depthImage       = nullptr;
    VkDeviceMemory depthImageMemory = nullptr;
    VkImageView    depthImageView   = nullptr;
    VkFormat       depthFormat      = VK_FORMAT_UNDEFINED;

    // Framebuffers
    std::vector<VkFramebuffer> framebuffers;

    // Commands
    VkCommandPool commandPool = nullptr;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers = {};

    // Sync
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores = {};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores = {};
    std::array<VkFence,     MAX_FRAMES_IN_FLIGHT> inFlightFences = {};
    uint32_t currentFrame = 0;

    // Clear color
    std::array<float, 4> clearColor = {0.1f, 0.1f, 0.15f, 1.0f};

    // ─── Triangle resources (Phase 3) ────────────────────────────────────────
    VmaAllocator allocator = nullptr;

    // Pipeline
    VkPipelineLayout pipelineLayout = nullptr;
    VkPipeline       pipeline       = nullptr;

    // Vertex buffer (GPU-local, uploaded via staging)
    AllocatedBuffer vertexBuffer{};

    // Uniform buffers (one per frame in flight, CPU-visible for easy updates)
    std::array<AllocatedBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers{};

    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout = nullptr;
    VkDescriptorPool      descriptorPool      = nullptr;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets = {};
};
