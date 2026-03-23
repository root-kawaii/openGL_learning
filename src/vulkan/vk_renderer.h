#pragma once

#include "vk_context.h"
#include "vk_buffer.h"
#include "vk_texture.h"
#include "../rhi/vulkan/vk_mesh_data.h"
#include <vector>
#include <array>
#include <memory>

#include <glm/glm.hpp>

// Forward declare — we don't want to pull in the full model.h here
class Model;

// Maximum number of frames that can be rendered concurrently.
static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// UBO matching the shader's UniformBufferObject
struct MVPUniform {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// ─── VulkanModelData ─────────────────────────────────────────────────────────
//
// Holds all Vulkan-side GPU resources for a loaded Model:
//   - Per-mesh vertex/index buffers (VkMeshData via RHI)
//   - Per-mesh diffuse textures (VulkanTexture)
//   - Per-mesh descriptor sets (one per frame in flight)
//
// This is separate from the Model itself — Model owns the CPU/GL data,
// VulkanModelData owns the Vulkan GPU data. This clean separation is the
// RHI's job: each backend manages its own resources independently.

struct VulkanMeshGPUData {
    std::unique_ptr<VkMeshData> buffers;
    VulkanTexture               diffuseTexture{};
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets = {};
};

struct VulkanModelData {
    std::vector<VulkanMeshGPUData> meshes;
};

class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // Initialize renderer with an already-initialized VulkanContext
    bool init(VulkanContext* context);

    // Upload a Model's mesh/texture data to Vulkan GPU resources
    bool loadModel(Model* model);

    // Draw a frame — renders the textured quad (or loaded model if available)
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
    bool createModelPipelineAndDescriptors();

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
    // imageAvailable + fences: per frame-in-flight (CPU/GPU pacing)
    // renderFinished: per swapchain image (prevents semaphore reuse before present completes)
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores = {};
    std::vector<VkSemaphore> renderFinishedSemaphores;   // one per swapchain image
    std::array<VkFence,     MAX_FRAMES_IN_FLIGHT> inFlightFences = {};
    uint32_t currentFrame = 0;

    // Clear color
    std::array<float, 4> clearColor = {0.1f, 0.1f, 0.15f, 1.0f};

    // ─── Quad resources (Phase 3 → Phase 4) ──────────────────────────────────
    VmaAllocator allocator = nullptr;

    // Textured quad pipeline
    VkPipelineLayout pipelineLayout = nullptr;
    VkPipeline       pipeline       = nullptr;

    // Vertex + index buffers (GPU-local, uploaded via staging)
    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};
    uint32_t        indexCount = 0;

    // Uniform buffers (one per frame in flight, CPU-visible for easy updates)
    std::array<AllocatedBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers{};

    // Texture (Phase 4)
    VulkanTexture texture{};

    // Descriptors (textured quad)
    VkDescriptorSetLayout descriptorSetLayout = nullptr;
    VkDescriptorPool      descriptorPool      = nullptr;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets = {};

    // ─── Model resources (Phase 5 — RHI) ─────────────────────────────────────
    VkPipelineLayout modelPipelineLayout  = nullptr;
    VkPipeline       modelPipeline        = nullptr;
    VkDescriptorSetLayout modelDescriptorSetLayout = nullptr;
    VkDescriptorPool      modelDescriptorPool      = nullptr;
    // Per-frame UBOs for model rendering (shared across all meshes)
    std::array<AllocatedBuffer, MAX_FRAMES_IN_FLIGHT> modelUniformBuffers{};

    // Fallback white texture for meshes without a diffuse texture
    VulkanTexture whiteTexture{};

    // Uploaded model data
    std::unique_ptr<VulkanModelData> modelData;
    bool hasModel = false;
};
