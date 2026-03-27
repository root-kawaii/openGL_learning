#pragma once

#include "vk_context.h"
#include "vk_buffer.h"
#include "vk_texture.h"
#include "../rhi/vulkan/vk_mesh_data.h"
#include <vector>
#include <array>
#include <memory>

#include <glm/glm.hpp>

// Forward declarations
class Model;
struct ImGuiContext;

// Maximum number of frames that can be rendered concurrently.
static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// Per-frame UBO — updated once per frame, shared across all meshes
struct FrameUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 lightPos;  // xyz = position, w = unused
};

// Shadow pass UBO — light-space matrix only
struct LightUBO {
    glm::mat4 lightSpaceMatrix;
};

// Per-object push constant — model matrix (updated per draw call, no rebind)
struct ModelPushConstant {
    glm::mat4 model;
};

// ID buffer push constant — model matrix + object ID for mouse picking
struct IDPushConstant {
    glm::mat4 model;
    uint32_t  objectID;
};

// Bone matrices UBO — one per skeleton, shared across all meshes of a model
static constexpr int MAX_BONES = 100;
struct BoneUBO {
    glm::mat4 bones[MAX_BONES];  // 6400 bytes
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

    // Set camera matrices for the next frame (call before drawFrame)
    void setViewMatrix(const glm::mat4& view);
    void setProjectionMatrix(const glm::mat4& proj);
    void setModelTransform(const glm::mat4& model);

    // Handle window resize — recreates swapchain + framebuffers
    bool handleResize(uint32_t width, uint32_t height);

    // Cleanup
    void cleanup();

    // Set clear color (RGBA, 0.0-1.0)
    void setClearColor(float r, float g, float b, float a = 1.0f);

    // ID buffer for mouse picking (Phase 11, Part 2)
    // Returns the object ID at the given pixel coordinates (0 = background/no object).
    // Synchronous: renders the ID buffer and reads back the pixel in one call.
    uint32_t getObjectIdAtPixel(int x, int y);

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
    bool createShadowResources();
    bool createSkyboxResources();
    bool initImGui();
    bool createIDBufferResources();

    void cleanupDepthResources();
    void cleanupIDBufferResources();
    void cleanupShadowResources();
    void cleanupSkyboxResources();
    void cleanupImGui();
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
    Model* loadedModel = nullptr;
    bool hasModel = false;

    // Bone animation (Phase 10)
    std::array<AllocatedBuffer, MAX_FRAMES_IN_FLIGHT> boneUniformBuffers{};

    // Ground plane (shadow receiver)
    AllocatedBuffer groundVertexBuffer{};
    AllocatedBuffer groundIndexBuffer{};
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> groundDescriptorSets = {};

    // ─── Shadow map resources (Phase 7) ─────────────────────────────────────
    static constexpr uint32_t SHADOW_MAP_SIZE = 2048;
    VkImage        shadowImage       = nullptr;
    VmaAllocation  shadowAllocation  = nullptr;
    VkImageView    shadowImageView   = nullptr;
    VkSampler      shadowSampler     = nullptr;  // comparison sampler for PCF
    VkRenderPass   shadowRenderPass  = nullptr;
    VkFramebuffer  shadowFramebuffer = nullptr;
    VkPipelineLayout shadowPipelineLayout = nullptr;
    VkPipeline       shadowPipeline       = nullptr;
    VkDescriptorSetLayout shadowDescriptorSetLayout = nullptr;
    VkDescriptorPool      shadowDescriptorPool      = nullptr;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> shadowDescriptorSets = {};
    std::array<AllocatedBuffer, MAX_FRAMES_IN_FLIGHT> shadowUniformBuffers{};

    // ─── Skybox resources (Phase 9) ──────────────────────────────────────────
    VkPipelineLayout skyboxPipelineLayout       = nullptr;
    VkPipeline       skyboxPipeline             = nullptr;
    VkDescriptorSetLayout skyboxDescriptorSetLayout = nullptr;
    VkDescriptorPool      skyboxDescriptorPool      = nullptr;
    VkDescriptorSet       skyboxDescriptorSet        = nullptr;
    AllocatedBuffer       skyboxVertexBuffer{};
    VulkanTexture         skyboxCubemap{};
    bool                  hasSkybox = false;

    // ─── ID buffer resources (Phase 11, Part 2 — mouse picking) ──────────────
    VkImage        idImage       = nullptr;
    VmaAllocation  idAllocation  = nullptr;
    VkImageView    idImageView   = nullptr;
    VkImage        idDepthImage       = nullptr;
    VmaAllocation  idDepthAllocation  = nullptr;
    VkImageView    idDepthImageView   = nullptr;
    VkRenderPass   idRenderPass  = nullptr;
    VkFramebuffer  idFramebuffer = nullptr;
    VkPipelineLayout idPipelineLayout = nullptr;
    VkPipeline       idPipeline       = nullptr;
    VkDescriptorSetLayout idDescriptorSetLayout = nullptr;
    VkDescriptorPool      idDescriptorPool      = nullptr;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> idDescriptorSets = {};
    AllocatedBuffer idStagingBuffer{};  // GPU→CPU readback of picked pixel
    uint32_t idBufferWidth  = 0;
    uint32_t idBufferHeight = 0;
    uint32_t lastPickedID   = 0;      // Last result from getObjectIdAtPixel()
    // Debug overlay: renders false-colored objects on main scene
    VkPipelineLayout idDebugPipelineLayout = nullptr;
    VkPipeline       idDebugPipeline       = nullptr;
    bool             showIDDebugOverlay    = false;

    // ─── ImGui resources (Phase 11) ──────────────────────────────────────────
    ImGuiContext* imguiContext = nullptr;
    bool imguiInitialized = false;

    // Camera state (set by main loop each frame)
    glm::mat4 currentView  = glm::mat4(1.0f);
    glm::mat4 currentProj  = glm::mat4(1.0f);
    glm::mat4 currentModel = glm::mat4(1.0f);
};
