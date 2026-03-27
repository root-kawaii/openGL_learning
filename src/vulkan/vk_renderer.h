#pragma once

#include "vk_context.h"
#include "vk_buffer.h"
#include "vk_texture.h"
#include "../rhi/vulkan/vk_mesh_data.h"
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

// Forward declarations
class Model;
class Scene;
struct ImGuiContext;

// Gizmo operation enum (used by Vulkan ImGuizmo panel)
enum class GizmoOp { Translate, Rotate, Scale };

// Maximum number of frames that can be rendered concurrently.
static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// Per-frame UBO — updated once per frame, shared across all meshes
struct PointLight {
    glm::vec4 position;  // xyz = world position, w = unused
    glm::vec4 color;     // xyz = RGB color, w = intensity
};

static constexpr int MAX_POINT_LIGHTS = 4;

struct FrameUBO {
    glm::mat4  view;
    glm::mat4  proj;
    glm::mat4  lightSpaceMatrix;
    glm::vec4  lightPos;   // xyz = shadow-casting directional light, w = unused
    glm::vec4  viewPos;    // xyz = camera position, w = unused
    // Extra point lights for PBR (trailing fields — ignored by non-PBR shaders)
    PointLight pointLights[MAX_POINT_LIGHTS];
    int        numPointLights;
    float      _pad[3];
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

// PBR push constant — model matrix + material scalars (Phase 14)
struct PBRPushConstant {
    glm::mat4 model;
    float     metallicVal;
    float     roughnessVal;
    uint32_t  hasNormalMap;
    uint32_t  _pad;
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
    // PBR textures (Phase 14)
    VulkanTexture               normalTexture{};
    VulkanTexture               metallicTexture{};
    VulkanTexture               roughnessTexture{};
    bool                        hasPBR = false;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> pbrDescriptorSets = {};
};

struct VulkanModelData {
    std::vector<VulkanMeshGPUData> meshes;
    std::array<AllocatedBuffer, MAX_FRAMES_IN_FLIGHT> boneBuffers{};
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

    // Upload all unique models from a scene and set the active scene for rendering
    bool loadScene(Scene* scene);

    // Swap active scene pointer without re-uploading models
    void setScene(Scene* scene) { currentScene = scene; }

    // Draw a frame — renders the textured quad (or loaded model if available)
    bool drawFrame();

    // Set camera matrices for the next frame (call before drawFrame)
    void setViewMatrix(const glm::mat4& view);
    void setProjectionMatrix(const glm::mat4& proj);

    // Handle window resize — recreates swapchain + framebuffers
    bool handleResize(uint32_t width, uint32_t height);

    // Render text during an active frame (call between beginFrame / drawFrame helpers, or from ImGui section).
    // x, y are screen pixels (top-left origin). scale=1.0 = native atlas size.
    // color is RGBA 0-1.
    void renderText(VkCommandBuffer cmd, const std::string& text,
                    float x, float y, float scale, glm::vec4 color);

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
    bool createMSAAResources();
    void cleanupMSAAResources();
    VkSampleCountFlagBits getMaxUsableSampleCount();
    bool recreateForMSAAChange(VkSampleCountFlagBits newSamples);
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createTriangleResources();
    bool createDescriptorSets();
    bool createModelPipelineAndDescriptors();
    bool createPBRPipelineAndDescriptors();
    bool createShadowResources();
    bool createSkyboxResources();
    bool initImGui();
    bool createIDBufferResources();
    bool createGridResources();
    bool createGrassResources();
    bool createWaterResources();
    bool createUIResources();
    bool createGroundPlane();

    void cleanupDepthResources();
    void cleanupIDBufferResources();
    void cleanupShadowResources();
    void cleanupSkyboxResources();
    void cleanupImGui();
    void cleanupFramebuffers();
    void cleanupGridResources();
    void cleanupGrassResources();
    void cleanupWaterResources();
    void cleanupUIResources();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findDepthFormat();

    VulkanContext* ctx = nullptr;

    // MSAA
    VkSampleCountFlagBits msaaSamples          = VK_SAMPLE_COUNT_1_BIT;
    VkSampleCountFlagBits msaaMaxSamples       = VK_SAMPLE_COUNT_1_BIT; // queried once at init
    bool                  pendingMSAAToggle    = false;
    VkImage               msaaColorImage    = nullptr;
    VkDeviceMemory        msaaColorMemory   = nullptr;
    VkImageView           msaaColorView     = nullptr;

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

    // Scene and multi-model GPU data (keyed by Model*)
    std::unordered_map<Model*, std::unique_ptr<VulkanModelData>> sceneModels;
    Scene* currentScene = nullptr;

    // Bone animation (Phase 10) — renderer-level UBO for shadow/ID descriptor sets (identity)
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

    // ─── PBR pipeline resources (Phase 14) ───────────────────────────────────
    VkPipelineLayout      pbrPipelineLayout       = nullptr;
    VkPipeline            pbrPipeline             = nullptr;
    VkDescriptorSetLayout pbrDescriptorSetLayout  = nullptr;
    VkDescriptorPool      pbrDescriptorPool       = nullptr;
    // 1x1 flat normal map fallback (0.5, 0.5, 1.0 = tangent-space up)
    VulkanTexture         flatNormalTexture{};
    // 1x1 metallic fallback (black = 0.0)
    VulkanTexture         blackTexture{};

    // ─── Grid resources (Phase 12) ───────────────────────────────────────────
    VkPipelineLayout gridPipelineLayout          = nullptr;
    VkPipeline       gridPipeline                = nullptr;
    VkDescriptorSetLayout gridDescriptorSetLayout = nullptr;
    VkDescriptorPool      gridDescriptorPool      = nullptr;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> gridDescriptorSets = {};
    bool showGrid = true;

    // ─── Grass resources (Phase 17) ──────────────────────────────────────────
    VkPipelineLayout      grassPipelineLayout      = nullptr;
    VkPipeline            grassPipeline            = nullptr;
    VkDescriptorSetLayout grassDescriptorSetLayout = nullptr;
    VkDescriptorPool      grassDescriptorPool      = nullptr;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> grassDescriptorSets = {};

    // Pre-built blade mesh (single blade, shared across all instances)
    AllocatedBuffer grassBladeVertexBuffer{};
    AllocatedBuffer grassBladeIndexBuffer{};
    uint32_t        grassBladeIndexCount = 0;

    // Per-instance data buffer (CPU-visible, updated each frame for culling)
    static constexpr uint32_t MAX_GRASS_INSTANCES = 20000;
    AllocatedBuffer grassInstanceBuffer{};
    uint32_t        grassInstanceCount = 0;

    // Grass texture (fallback to solid green if missing)
    VulkanTexture   grassTexture{};
    bool            showGrass = false;

    // ─── Water resources (Phase 16) ──────────────────────────────────────────
    VkPipelineLayout      waterPipelineLayout      = nullptr;
    VkPipeline            waterPipeline            = nullptr;
    VkDescriptorSetLayout waterDescriptorSetLayout = nullptr;
    VkDescriptorPool      waterDescriptorPool      = nullptr;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> waterDescriptorSets = {};
    AllocatedBuffer       waterVertexBuffer{};
    AllocatedBuffer       waterIndexBuffer{};
    uint32_t              waterIndexCount = 0;
    bool                  showWater       = false;  // toggled via ImGui

    // Reflection offscreen image (half-res, captures scene without water)
    VkImage        reflectionImage        = nullptr;
    VmaAllocation  reflectionAllocation   = nullptr;
    VkImageView    reflectionImageView    = nullptr;
    VkSampler      reflectionSampler      = nullptr;
    VkImage        reflectionDepthImage   = nullptr;
    VmaAllocation  reflectionDepthAlloc   = nullptr;
    VkImageView    reflectionDepthView    = nullptr;
    VkRenderPass   reflectionRenderPass   = nullptr;
    VkFramebuffer  reflectionFramebuffer  = nullptr;

    // Fallback 1×1 blue-ish texture used when no water normal map is on disk
    VulkanTexture  waterNormalTexture{};

    // ─── UI text + sprite resources (Phase 15) ───────────────────────────────
    // Both text and sprite share the same descriptor set layout (one sampler2D binding).
    VkDescriptorSetLayout uiDescriptorSetLayout = nullptr;
    VkDescriptorPool      uiDescriptorPool      = nullptr;

    // Text pipeline (R8 glyph atlas)
    VkPipelineLayout uiTextPipelineLayout = nullptr;
    VkPipeline       uiTextPipeline       = nullptr;
    VulkanTexture    glyphAtlasTexture{};
    VkDescriptorSet  glyphAtlasDescriptorSet = VK_NULL_HANDLE;

    // Sprite pipeline (RGBA texture)
    VkPipelineLayout uiSpritePipelineLayout = nullptr;
    VkPipeline       uiSpritePipeline       = nullptr;

    // Dynamic vertex buffer for UI quads (updated per-frame, CPU-visible)
    static constexpr uint32_t UI_MAX_QUADS = 4096;
    AllocatedBuffer uiVertexBuffer{};

    // Glyph metrics cached from FreeType atlas build
    struct GlyphInfo {
        float u0, v0, u1, v1;  // UV coords in atlas (normalized 0-1)
        int   bearingX, bearingY;
        int   advance;
        int   width, height;
    };
    GlyphInfo glyphs[128] = {};
    int       glyphCellW  = 0;  // atlas cell width in pixels
    int       glyphCellH  = 0;  // atlas cell height in pixels
    bool      hasUIResources = false;

    // ─── ImGui resources (Phase 11) ──────────────────────────────────────────
    ImGuiContext* imguiContext = nullptr;
    bool imguiInitialized = false;

    // ─── Frame timing (FPS display) ───────────────────────────────────────────
    double lastFrameTime = 0.0;
    float  displayFPS    = 0.0f;
    uint32_t vkDrawCalls = 0;  // reset and counted each frame

    // ─── Scene entity selection + gizmo (re-added with real functionality) ───
    int     selectedObjectIndex = -1;  // index into currentScene->getGameObjects()
    GizmoOp gizmoOp    = GizmoOp::Translate;
    bool    gizmoWorld = true;

    // Camera state (set by main loop each frame)
    glm::mat4 currentView  = glm::mat4(1.0f);
    glm::mat4 currentProj  = glm::mat4(1.0f);
};
