#pragma once

#include "vk_context.h"
#include "vk_buffer.h"
#include "vk_texture.h"
#include "../rhi/vulkan/vk_mesh_data.h"
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <functional>

#include <glm/glm.hpp>

// Forward declarations
class Model;
class Scene;
struct ImGuiContext;

struct FrameTimingStats {
    float wallFrameMs  = 0.0f;  // frame-to-frame cadence, includes present pacing
    float cpuSubmitMs  = 0.0f;  // CPU time spent building/submitting this frame
    float gpuFrameMs   = 0.0f;  // GPU execution time from timestamp queries
    float presentMs    = 0.0f;  // time spent inside vkQueuePresentKHR
    bool  gpuSupported = false;
    bool  gpuValid     = false;
};

// Gizmo operation enum (used by Vulkan ImGuizmo panel)
enum class GizmoOp { Translate, Rotate, Scale };

// Maximum number of frames that can be rendered concurrently.
static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// Per-frame UBO — updated once per frame, shared across all meshes
struct PointLight {
    glm::vec4 position;  // xyz = world position, w = unused
    glm::vec4 color;     // xyz = RGB color, w = intensity
};

static constexpr int MAX_POINT_LIGHTS = 16;

struct FrameUBO {
    glm::mat4  view;
    glm::mat4  proj;
    glm::mat4  lightSpaceMatrix;
    glm::vec4  lightPos;   // xyz = shadow-casting directional light, w = intensity
    glm::vec4  viewPos;    // xyz = camera position, w = ambient intensity
    // Extra point lights for PBR (trailing fields — ignored by non-PBR shaders)
    PointLight pointLights[MAX_POINT_LIGHTS];
    int        numPointLights;
    float      _pad[3];
    // ─── Stylized atmosphere block (trailing — older shaders ignore it) ───────
    // All vec4 to keep std140 layout trivial. Shaders declare the prefix they need.
    glm::vec4  fogColor;    // rgb = fog tint, a = strength (0 = fog off)
    glm::vec4  fogParams;   // x = distNear, y = distFar, z = heightStart, w = heightFalloff
    glm::vec4  skyTop;      // rgb = zenith color
    glm::vec4  skyHorizon;  // rgb = horizon color
    glm::vec4  sunDir;      // xyz = direction toward sun (normalized), w = time seconds
    glm::vec4  style;       // x = lightingMode(0 real,1 banded), y = bands, z = rimStrength, w = depthMax
    glm::vec4  rimColor;    // rgb = rim tint, w = wind strength
    glm::mat4  pointLightSpace[16];  // perspective light-space matrix per shadow layer
    glm::vec4  pointShadowInfo[16];  // x = FrameUBO light index this layer shadows, y = active
    glm::vec4  pointShadowMeta;      // x = active count, y = depth bias
};

// Blit / post-process push constant (fits in the 128-byte guaranteed range).
// Packed as vec4s so std430 alignment is trivial.
struct BlitPush {
    glm::vec4 p0;          // x=paletteSize, y=ditherStrength, z=saturation, w=contrast
    glm::vec4 p1;          // x=brightness,  y=temperature,    z=tint,       w=vignette
    glm::vec4 p2;          // x=outlineStr,  y=godrayStr,       z=bloomStr,   w=dofStr
    glm::vec4 outlineColor;// rgb = outline color, w=aoStrength
    glm::vec4 p4;          // x=sunScreenX,  y=sunScreenY,      z=time,       w=texelScale(=pixelArtScale)
    glm::vec4 p5;          // x=lutStrength, yzw=sunColor
};

struct VolumetricUBO {
    glm::mat4 invViewProj;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 camPos;       // xyz = camera world pos, w = depthMax
    glm::vec4 sunDir;       // xyz = direction TO sun, w = volumetric strength
};

// Shadow pass UBO — light-space matrix only
struct LightUBO {
    glm::mat4 lightSpaceMatrix;
};

// Per-object push constant — model matrix (updated per draw call, no rebind)
struct ModelPushConstant {
    glm::mat4 model;
    glm::vec4 albedoTint;
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
    glm::vec4 albedoTint;
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
    // NOTE: boneBuffers moved to VulkanInstanceData (per-instance, not per-model)
};

// Per-instance GPU data — owns bone UBOs and per-mesh descriptor sets that
// reference this instance's bones. Created for every animated scene object so
// two characters with the same .glb play independent animation states.
struct VulkanInstanceData {
    std::array<AllocatedBuffer, MAX_FRAMES_IN_FLIGHT> boneBuffers{};
    // Per-mesh, per-frame descriptor sets (regular + PBR) using instance bones
    std::vector<std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>> meshDescSets;
    std::vector<std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>> pbrDescSets;
    // Shadow-pass descriptor sets — bind instance bone buffer so animated objects
    // cast shadows matching their current animation pose (not the zero/T-pose).
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> shadowDescSets = {};
    // ID-pass descriptor sets — same reason: animated objects need live bones
    // or their vertices collapse to the origin and the pixel returns 0 (miss).
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> idDescSets = {};
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

    // Draw a frame. Optional callback is invoked inside the Vulkan ImGui frame
    // so callers (e.g. main.cpp) can push ENGINE panels, level editor, pause
    // menu etc. into the Vulkan window without needing a separate context switch.
    bool drawFrame(std::function<void()> engineCallback = nullptr);

    // Expose the Vulkan ImGui context so external code can push content into it
    ImGuiContext* getImGuiContext() const { return imguiContext; }
    const FrameTimingStats& getFrameTimingStats() const { return frameTimingStats; }

    // Set camera matrices for the next frame (call before drawFrame)
    void setViewMatrix(const glm::mat4& view);
    void setProjectionMatrix(const glm::mat4& proj);
    void setGridVisible(bool visible) { gridVisibleInCurrentMode = visible; }
    void setSkyboxVisible(bool visible) { skyboxVisibleInCurrentMode = visible; }
    void setDirectionalLightEnabled(bool enabled) { directionalLightEnabled = enabled; }
    void setAmbientStrength(float ambient) { ambientStrength = ambient; }

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
    // Draws every scene model (static + animated instances) with the given pipelines.
    // Shared by the main scene pass and the water reflection capture. enableCull does
    // main-camera frustum culling; collectStats updates the per-frame draw counters.
    void drawSceneModels(VkCommandBuffer cmd, VkPipeline pbrPipe, VkPipeline modelPipe,
                         bool enableCull, bool collectStats);

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
    bool createFrameTimingQueries();
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
    bool createPixelArtResources();
    // Create per-instance bone buffers + descriptor sets for an animated object
    bool createInstanceData(int objIdx, Model* model);

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
    void cleanupPixelArtResources();

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

    // ─── Pixel art post-process ───────────────────────────────────────────────
    // Renders the scene at 1/pixelArtScale resolution into paColorImage, then
    // upscales to the swapchain with NEAREST filter + posterization blit.
    // ImGui/UI text stay at full resolution in the normal renderPass.
    bool  pixelArtEnabled  = true;
    int   pixelArtScale    = 4;     // render at 1/4 resolution
    float paletteSize      = 12.0f; // color quantization steps per channel

    // ─── Stylized "Look / Atmosphere" controls (all toggleable; realism paths kept) ──
    struct LookSettings {
        // Fog
        bool      fogEnabled       = true;
        glm::vec3 fogColor         = {0.62f, 0.70f, 0.80f};
        float     fogDistNear      = 25.0f;   // start further out — keep nearby scene crisp
        float     fogDistFar       = 110.0f;
        float     fogHeightStart   = 4.0f;
        float     fogHeightFalloff = 0.15f;
        float     fogStrength      = 0.7f;
        // Lighting style
        int       lightingMode     = 1;     // 0 = realistic, 1 = banded/toon
        float     bands            = 4.0f;
        float     rimStrength      = 0.35f;
        glm::vec3 rimColor         = {1.0f, 0.85f, 0.6f};
        // Sky
        float     skyStylize       = 1.0f;  // 0 = cubemap, 1 = gradient
        // Wind
        float     windStrength     = 1.0f;
        // Post-process (blit)
        float     ditherStrength   = 0.6f;
        float     saturation       = 1.12f;
        float     contrast         = 1.06f;
        float     brightness       = 0.0f;
        float     temperature      = 0.10f; // + = warm
        float     tint             = 0.0f;
        float     vignette         = 0.35f;
        float     outlineStrength  = 0.5f;
        glm::vec3 outlineColor     = {0.06f, 0.05f, 0.09f};
        float     godrayStrength   = 0.60f;
        float     bloomStrength     = 0.12f;
        float     dofStrength      = 0.0f;
        float     aoStrength       = 0.0f;  // depth-proxy AO — off by default (only helps recesses, not flat corners)
        float     lutStrength      = 0.0f;  // 3D color-grade LUT blend (0 = off; needs assets/lut.png)
        float     depthMax         = 80.0f; // normalizer for depth-in-alpha
        // Time of day
        bool      todEnabled       = true;
        bool      todAuto          = false;
        float     timeOfDay        = 0.38f; // 0..1 (0=midnight,0.25=sunrise,0.5=noon,0.75=sunset)
        float     todSpeed         = 0.01f;
    } look;
    // Sun direction + colors derived from time-of-day each frame
    glm::vec3 sunDirWorld   = glm::normalize(glm::vec3(-1.0f, 4.0f, 1.0f));
    glm::vec3 sunColor      = glm::vec3(1.0f);
    float     sunIntensity  = 1.0f;
    glm::vec3 skyTopColor   = glm::vec3(0.35f, 0.55f, 0.85f);
    glm::vec3 skyHorizColor = glm::vec3(0.8f, 0.85f, 0.9f);
    glm::vec2 sunScreenPos  = glm::vec2(0.5f, 0.3f);
    void updateTimeOfDay(float dt);   // recompute sun/sky/fog from look.timeOfDay

    // ─── Atmosphere dust-mote particles (Tier 3) ─────────────────────────────
    bool                  showParticles = true;
    VkPipelineLayout      particlePipelineLayout      = nullptr;
    VkPipeline            particlePipeline            = nullptr;
    VkDescriptorSetLayout particleDescriptorSetLayout = nullptr;
    VkDescriptorPool      particleDescriptorPool      = nullptr;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> particleDescriptorSets = {};
    static constexpr uint32_t MAX_PARTICLES = 2048;
    AllocatedBuffer       particleInstanceBuffer{};   // CPU-visible, per-frame
    uint32_t              particleCount = 0;
    std::vector<glm::vec4> particleSeeds;             // xyz = base pos, w = phase
    bool createParticleResources();
    void cleanupParticleResources();

    // ─── Emissive light-glow sprites (Tier 3 — torch glow) ───────────────────
    // Additive warm billboards at each light; the bloom pass turns them into halos.
    // Reuses the particle descriptor layout (FrameUBO only).
    bool                  showLightGlow = true;
    float                 glowSize      = 1.4f;   // billboard radius scale
    float                 glowIntensity = 1.0f;   // brightness multiplier
    VkPipelineLayout      glowPipelineLayout = nullptr;
    VkPipeline            glowPipeline       = nullptr;
    static constexpr uint32_t MAX_GLOWS = 64;
    AllocatedBuffer       glowInstanceBuffer{};    // CPU-visible, refilled per frame
    uint32_t              glowCount = 0;
    bool createGlowResources();
    void cleanupGlowResources();

    // Low-res 1-sample color image (SAMPLED_BIT — blit source)
    VkImage        paColorImage   = nullptr;
    VmaAllocation  paColorAlloc   = nullptr;
    VkImageView    paColorView    = nullptr;
    // Low-res MSAA color (only created when msaaSamples > 1)
    VkImage        paMsaaImage    = nullptr;
    VmaAllocation  paMsaaAlloc    = nullptr;
    VkImageView    paMsaaView     = nullptr;
    // Low-res depth (msaaSamples)
    VkImage        paDepthImage   = nullptr;
    VmaAllocation  paDepthAlloc   = nullptr;
    VkImageView    paDepthView    = nullptr;
    // Render pass: same structure as renderPass, finalLayout=SHADER_READ_ONLY
    VkRenderPass   paScenePass    = nullptr;
    VkFramebuffer  paFramebuffer  = nullptr;
    // NEAREST sampler + blit descriptor/pipeline (draws paColorImage → swapchain)
    VkSampler      pixelArtSampler     = nullptr;
    VkDescriptorSetLayout blitDescLayout = nullptr;
    VkDescriptorPool      blitDescPool   = nullptr;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> blitDescSets = {};
    VkPipelineLayout blitPipelineLayout = nullptr;
    VkPipeline       blitPipeline       = nullptr;

    // ─── Color-grade 3D LUT (Tier 1.1) ───────────────────────────────────────
    // Loaded from a strip-format LUT PNG (N slices laid horizontally → N×N×N
    // cube). Falls back to a neutral identity LUT so the blit binding is always
    // valid. Sampled in quad.frag as a sampler3D, blended by look.lutStrength.
    VkImage       lutImage   = nullptr;
    VmaAllocation lutAlloc   = nullptr;
    VkImageView   lutView    = nullptr;
    VkSampler     lutSampler = nullptr;
    bool          lutFromFile = false;
    bool createLUTResources();   // idempotent — created once, survives resize
    void cleanupLUTResources();

    // Commands
    VkCommandPool commandPool = nullptr;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers = {};

    // GPU frame timing (per-frame timestamp query pair: start/end)
    VkQueryPool frameTimingQueryPool = nullptr;
    bool gpuTimestampsSupported = false;
    double gpuTimestampPeriodNs = 0.0;
    std::array<bool, MAX_FRAMES_IN_FLIGHT> frameTimingQueryReady = {};

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

    // Per-instance GPU data (keyed by scene object index).
    // Only animated objects get an entry; static objects use the model's
    // identity-bone descriptor sets.
    std::unordered_map<int, std::unique_ptr<VulkanInstanceData>> instanceData;

    // Bone animation (Phase 10) — renderer-level UBO for ground/shadow/ID descriptor sets (identity)
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

    // ─── Point-light (spot) shadow — strongest point light casts a perspective shadow ──
    // Reuses shadowRenderPass + shadowPipeline; sampled via a shared set=1 sampler so
    // the per-mesh set=0 descriptors don't need editing.
    static constexpr uint32_t POINT_SHADOW_SIZE = 1024;
    static constexpr int      MAX_POINT_SHADOWS = 16;  // shadow-caster budget (culled per frame)
    VkImage        pointShadowImage       = nullptr;   // D32 2D array, one layer per shadow light
    VmaAllocation  pointShadowAlloc       = nullptr;
    VkImageView    pointShadowArrayView   = nullptr;   // sampler2DArray view (sampling)
    std::array<VkImageView,   MAX_POINT_SHADOWS> pointShadowLayerViews = {}; // per-layer (render)
    std::array<VkFramebuffer, MAX_POINT_SHADOWS> pointShadowFramebuffers = {};
    VkSampler      pointShadowSampler     = nullptr;
    // Per layer × per frame: light-space UBO + render descriptor set
    std::array<std::array<AllocatedBuffer, MAX_FRAMES_IN_FLIGHT>, MAX_POINT_SHADOWS> pointShadowUBOs{};
    std::array<std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>, MAX_POINT_SHADOWS> pointShadowRenderSets{};
    VkDescriptorSetLayout pointShadowSampleLayout = nullptr; // set=1 in model/pbr layouts
    VkDescriptorPool      pointShadowSamplePool   = nullptr;
    VkDescriptorSet       pointShadowSampleSet    = VK_NULL_HANDLE;
    std::array<glm::mat4, MAX_POINT_SHADOWS> pointLightSpaceMatrices{};
    std::array<int, MAX_POINT_SHADOWS>       pointShadowLightIdx{};  // FrameUBO light index per layer
    int       pointShadowCount   = 0;       // active shadow-casting lights this frame
    bool      pointShadowEnabled = true;
    float     pointShadowMaxDist = 70.0f;   // cull: only lights within this range cast shadows
    bool      pointShadowInitialized = false; // one-time layout transition done
    bool createPointShadowResources();
    void cleanupPointShadowResources();

    // ─── Volumetric lighting UBO (blit pass binding 3) ─────────────────────
    std::array<AllocatedBuffer, MAX_FRAMES_IN_FLIGHT> volUBOBuffers{};

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
    bool gridVisibleInCurrentMode = true;
    bool skyboxVisibleInCurrentMode = true;
    bool directionalLightEnabled = true;
    float ambientStrength = 0.03f;

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
    bool                  showWater       = true;   // toggled via ImGui
    float                 waterLevel      = 0.15f;  // Y height of pool surface

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
    VkPipeline     reflModelPipeline      = nullptr;
    VkPipeline     reflPbrPipeline        = nullptr;

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

    // ─── Frustum culling (main camera pass) ──────────────────────────────────
    bool     frustumCullEnabled = true;
    uint32_t culledObjects      = 0;   // objects skipped this frame (stat)
    FrameTimingStats frameTimingStats{};

    // ─── Scene entity selection + gizmo (re-added with real functionality) ───
    int     selectedObjectIndex = -1;  // index into currentScene->getGameObjects()
    GizmoOp gizmoOp    = GizmoOp::Translate;
    bool    gizmoWorld = true;

    // Camera state (set by main loop each frame)
    glm::mat4 currentView  = glm::mat4(1.0f);
    glm::mat4 currentProj  = glm::mat4(1.0f);
};
