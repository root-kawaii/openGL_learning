#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../src/camera.h"
#include "../src/mesh.h"
#include "../src/shader_m.h"
#include "../src/texture.h"
#include "../src/game_object.h"
#include "../src/serialization_utilities.h"
#include "../src/scene.h"

#include <random>

// Forward declarations
// class Shader;
// class Texture;
class GameEntity;
struct Light;
// class Mesh;
// class Camera;

class Scene;

struct GridLineInstance
{
    glm::vec3 startPos;
    glm::vec3 direction;
    float thickness;
    float length;
    glm::vec3 color;
    float _padding; // Align to 16 bytes
};

struct RenderCommand
{
    Mesh *mesh;
    Shader *shader;
    glm::mat4 modelMatrix;
    std::vector<Texture *> textures;
    float distance; // for sorting
};

// struct Light
// {
//     glm::vec3 position;
//     glm::vec3 color;
//     float intensity;
//     // Add more light properties as needed
// };

struct GrassInstance
{
    glm::vec3 position;
    float rotation;
    float scale;
    glm::vec3 tint; // Individual grass blade tinting
};

class Game;      // Forward declaration
class UIManager; // Forward declaration

class RenderManager
{
private:
    Scene *currentScene;
    Game *gameInstance = nullptr;
    UIManager *uiManager = nullptr;

    float lastTimeSinceShaderReload = 0.0f;

    // Internal helper functions
    void bindTextures(const std::vector<Texture *> &textures);
    void unbindTextures();
    void setupShaderUniforms(Shader *shader, const glm::mat4 &modelMatrix);
    float calculateDistance(const glm::vec3 &position);

    // G-Buffer helper functions
    bool checkGBufferStatus();
    void createGBufferTextures();

    // MSAA G-Buffer helper functions
    bool checkMSAAGBufferStatus();
    void createMSAAGBufferTextures();

    void initializeGridBuffers();

    void setupIDBuffer();
    void debugIDBuffer();

    void sceneBuffersSetup();

    // Rendering queues
    std::vector<RenderCommand> opaqueQueue;
    std::vector<RenderCommand> transparentQueue;
    std::vector<RenderCommand> uiQueue;

    // Shader management
    std::unordered_map<std::string, std::shared_ptr<Shader>> shaders;
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
    // std::unordered_map<std::string, std::unique_ptr<Mesh>> meshes;

    // Lighting
    std::vector<Light> lights;
    glm::vec3 ambientLight;

    // Camera and matrices
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 lightSpaceMatrix;
    std::vector<glm::vec3> lightPositions = {
        // glm::vec3(-1.0f, 1.0f, 1.0f),
        glm::vec3(-1.0f, 1.0f, 10.0f),
        // glm::vec3(-1.0f, -1.0f, 1.0f),
        // glm::vec3(10.0f, -10.0f, 10.0f),
    };
    float near_plane = 1.0f, far_plane = 75.5f;
    float SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;

    // Render settings
    glm::vec4 clearColor;
    bool wireframeMode;
    bool depthTestEnabled;
    bool blendingEnabled;

    // Screen dimensions
    int screenWidth, screenHeight;

    // Statistics
    int drawCalls;
    int verticesRendered;
    int textureCounter;

    // Bone debug mode: 0=normal, 1=bone colors, 2=weight heatmap, 3=dominant bone
    int boneDebugMode;

    // G-Buffer members
    unsigned int gBuffer;
    unsigned int gPosition, gNormal, gAlbedoSpec, gDepth, gLinearDepth, gMetallic, gRoughness;
    bool gBufferInitialized;

    // MSAA G-Buffer members
    unsigned int msaaGBuffer;
    unsigned int msaaGPosition, msaaGNormal, msaaGAlbedoSpec, msaaGDepth, msaaGLinearDepth, msaaGMetallic, msaaGRoughness;
    bool msaaGBufferInitialized;
    int msaaSamples;

    unsigned int gridVAO = 0;
    unsigned int gridVBO = 0;         // Base line geometry
    unsigned int gridInstanceVBO = 0; // Instance data
    unsigned int gridShader = 0;
    bool gridInitialized = false;

    unsigned int IDFrameBuffer = 0;
    unsigned int idTexture = 0;
    unsigned int rboDepth = 0;
    int idBufferWidth = 0;
    int idBufferHeight = 0;
    bool useIntegerTexture = true;

    unsigned int grassVAO, grassVBO, grassInstanceVBO;
    std::vector<GrassInstance> grassInstances;
    bool grassInstancesGenerated = false;

    std::shared_ptr<Model> grassModel;
    bool grassModelLoaded = false;
    bool grassInstanced = false;

    // Texture cache
    std::unordered_map<std::string, unsigned int> textureCache;

    // ── Named PBR material library (GPU side) ─────────────────────────────────
    // Mirrors the JSON "materials" section; holds GL texture IDs for each map.
    // 0 = map not provided (shader falls back to uniform defaults).
    struct PBRMaterial
    {
        unsigned int albedo    = 0;
        unsigned int normal    = 0;
        unsigned int metallic  = 0;
        unsigned int roughness     = 0;
        unsigned int ao            = 0;
        unsigned int displacement  = 0; // height map for POM (slot 7)
    };
    std::unordered_map<std::string, PBRMaterial> pbrMaterials;

    std::shared_ptr<Model> arrowModel;
    std::shared_ptr<Model> lineModel;
    std::shared_ptr<Model> elModel;
    std::shared_ptr<Model> torchModel;
    glm::vec3 linesColor = glm::vec3(1.0f, 1.0f, 0.0f);

    unsigned int loadAndCacheTexture(const std::string &name, const std::string &path);
    void generateGrassInstances(const glm::vec3 &center, float radius, int density);
    void setupGrassInstancing();

    unsigned int skyboxVAO, skyboxVBO;
    unsigned int cubemapTexture;

    // Cached terrain texture IDs (loaded once, reused every frame)
    unsigned int t_grassDiff = 0, t_grassNor = 0;
    unsigned int t_stoneDiff = 0, t_stoneNor = 0;
    unsigned int t_rock2Diff = 0, t_rock2Nor = 0;
    // Optional PBR maps (0 = not loaded, shader falls back to defaults)
    unsigned int t_grassAO = 0, t_stoneAO = 0;
    unsigned int t_grassRough = 0, t_stoneRough = 0;

    // Track current paths so the UI can display them
    std::string terrainPaths[3][4]; // [slot][mapType]

    // Procedural water mesh (generated at runtime, covers full level)
    unsigned int waterVAO = 0;
    unsigned int waterVBO = 0;
    unsigned int waterEBO = 0;
    unsigned int waterIndexCount = 0;
    float waterYLevel = 0.0f;
    float waterHalfExtent = 100.0f;

    // Procedural ground mesh (flat seabed, 2 units below main level)
    unsigned int groundVAO = 0;
    unsigned int groundVBO = 0;
    unsigned int groundEBO = 0;
    unsigned int groundIndexCount = 0;
    float groundYLevel = -2.0f;
    float groundHalfExtent = 200.0f;

    // Procedural terrain mesh (FBM-displaced, toon-shaded, covers level footprint)
    unsigned int procTerrainVAO = 0;
    unsigned int procTerrainVBO = 0;
    unsigned int procTerrainEBO = 0;
    unsigned int procTerrainIndexCount = 0;

    // Planar reflection FBO — renders scene from mirrored camera for water reflections
    unsigned int reflectionFBO = 0;
    unsigned int reflectionTexture = 0;
    unsigned int reflectionDepthRBO = 0;

    // Clip plane sent to all scene vertex shaders; non-clipping by default
    glm::vec4 activeClipPlane = glm::vec4(0.0f, -1.0f, 0.0f, 1e6f);

    // Reflection VP matrix saved by renderReflectionPass, sent to water shader
    glm::mat4 reflectionVP = glm::mat4(1.0f);

    // Trajectory rendering control
    bool renderTrajectory = false; // Off by default for performance
    int trajectorySegments = 50;   // Reduced from 1000 for performance

    // ID buffer update control (only render when needed for mouse picking)
    bool needIDBufferUpdate = true; // Render on first frame

    // Rain particle system
    struct RainParticle
    {
        glm::vec3 pos;
    };
    static constexpr int RAIN_COUNT = 15000;
    unsigned int rainVAO = 0;
    unsigned int rainMeshVBO = 0;
    unsigned int rainInstanceVBO = 0;
    std::vector<RainParticle> rainParticles;
    std::vector<glm::vec3> rainInstanceData;
    std::mt19937 rainRng{123};
    bool rainEnabled = false;

    // Async animation futures — launched by prepareAllAnimations(), consumed by waitForAnimations()
    std::vector<std::future<void>> m_animFutures;

    // ── Tile instancing ────────────────────────────────────────────────────────
    struct TileInstance
    {
        glm::mat4 modelMatrix;
        float terrainType;
        float pad[3]{}; // align to 16 bytes
    };

    struct TileBatch
    {
        unsigned int instanceVBO = 0;
        bool setupDone = false;
    };

    std::unordered_map<std::string, TileBatch> tileBatches;
    void setupTileBatch(const std::string &modelPath, Model &model, size_t maxInstances);
    void renderTilesInstanced(const std::vector<std::shared_ptr<GameObject>> &objects,
                              const std::vector<glm::vec3> &lightPos,
                              const glm::mat4 &lightSpaceMatrix);
    void clearTileBatches();

    std::vector<glm::vec3> getReachableTilesForEntity(std::shared_ptr<GameEntity> entity);

public:
    RenderManager();
    ~RenderManager();

    Camera *currentCamera;

    unsigned int depthFBO;
    unsigned int depthTexture;

    // Initialization
    bool initialize(int width, int height);
    void cleanup();

    // Terrain texture hot-swap (slot 0=grass, 1=stone, 2=rock2 | map 0=diff, 1=nor, 2=ao, 3=rough)
    bool reloadTerrainSlot(int slot, int mapType, const std::string &path);
    std::string getTerrainSlotPath(int slot, int mapType) const;

    void setRes(int width, int height)
    {
        screenWidth = width;
        screenHeight = height;
    };

    void setTimeSinceLastShaderReload(float time) { lastTimeSinceShaderReload = time; };

    void setGame(Game *game) { gameInstance = game; }
    void setUIManager(UIManager *ui) { uiManager = ui; }
    float getTimeSinceLastShaderReload() const { return lastTimeSinceShaderReload; };
    void checkAndReloadShaders();

    // Light management
    void setLights(const std::vector<Light> &sceneLights);

    void setBoneDebugMode(int mode) { boneDebugMode = mode; };
    int getBoneDebugMode() const { return boneDebugMode; };

    void setScene(Scene *scene) { currentScene = scene; }

    // Frame management
    void beginFrame();
    void endFrame();
    void clear();
    void present();

    // G-Buffer management
    void setupGBuffer();
    void cleanupGBuffer();
    void bindGBuffer();
    void unbindGBuffer();
    void resizeGBuffer(int width, int height);

    // MSAA G-Buffer management
    void setupMSAAGBuffer(int samples = 8);
    void cleanupMSAAGBuffer();
    void bindMSAAGBuffer();
    void unbindMSAAGBuffer();
    void resolveMSAAGBuffer(); // Resolve MSAA to regular G-Buffer
    void resizeMSAAGBuffer(int width, int height);

    // Skybox management
    void setupSkybox();
    void cleanupSkybox();
    void setSkyboxTexture(unsigned int texture);
    void setSkyboxShader(Shader *shader);
    void renderSkybox();
    unsigned int loadCubemap(const std::vector<std::string> &faces);

    // G-Buffer texture getters
    unsigned int getGBuffer() const { return gBuffer; }
    unsigned int getPositionTexture() const { return gPosition; }
    unsigned int getNormalTexture() const { return gNormal; }
    unsigned int getAlbedoSpecTexture() const { return gAlbedoSpec; }
    unsigned int getDepthTexture() const { return gDepth; }
    unsigned int getLinearDepthTexture() const { return gLinearDepth; }
    unsigned int getMetallicTexture() const { return gMetallic; }
    unsigned int getRoughnessTexture() const { return gRoughness; }

    // MSAA G-Buffer texture getters
    unsigned int getMSAAGBuffer() const { return msaaGBuffer; }
    unsigned int getMSAAPositionTexture() const { return msaaGPosition; }
    unsigned int getMSAANormalTexture() const { return msaaGNormal; }
    unsigned int getMSAAAlbedoSpecTexture() const { return msaaGAlbedoSpec; }
    unsigned int getMSAADepthTexture() const { return msaaGDepth; }
    unsigned int getMSAALinearDepthTexture() const { return msaaGLinearDepth; }
    unsigned int getMSAAMetallicTexture() const { return msaaGMetallic; }
    unsigned int getMSAARoughnessTexture() const { return msaaGRoughness; }

    // Render queue management
    void submit(Mesh *mesh, Shader *shader, const glm::mat4 &modelMatrix,
                const std::vector<Texture *> &textures = {});
    void submitUI(Mesh *mesh, Shader *shader, const glm::mat4 &modelMatrix);
    void submitTransparent(Mesh *mesh, Shader *shader, const glm::mat4 &modelMatrix,
                           const std::vector<Texture *> &textures = {});

    // Rendering
    void renderScene();
    void renderOpaqueObjects();
    void renderTransparentObjects();
    void renderUI();

    // Resource management
    Shader *loadShader(const std::string &name, const std::string &vertexPath,
                       const std::string &fragmentPath);
    unsigned int loadTexture(const std::string &name, const char *path);
    Mesh *loadMesh(const std::string &name, const std::string &path);

    Shader *getShader(const std::string &name);
    void useShader(GameObject &gameObject, Shader *shader, std::vector<glm::vec3> &lightPos, glm::mat4 lightSpaceMatrix);
    Texture *getTexture(const std::string &name);
    Mesh *getMesh(const std::string &name);

    // Camera management
    void setCamera(Camera *camera);
    void setProjectionMatrix(glm::mat4 projectionMatrixArg) { projectionMatrix = projectionMatrixArg; };
    void setViewMatrix(glm::mat4 viewMatrixArg) { viewMatrix = viewMatrixArg; };
    void updateCameraMatrices();

    glm::mat4 getViewMatrix() { return viewMatrix; };
    glm::mat4 getProjectionMatrix() { return projectionMatrix; };

    // Lighting
    void addLight(const Light &light);
    void clearLights();
    void setAmbientLight(const glm::vec3 &color);
    void updateLightUniforms(Shader *shader);

    // Render state management
    void setClearColor(const glm::vec4 &color);
    void setWireframeMode(bool enabled);
    void enableDepthTest(bool enabled);
    void enableBlending(bool enabled);
    void setViewport(int width, int height);

    // Utility functions
    void sortTransparentQueue();
    void setupRenderStates();
    void resetRenderStates();

    // Debug and statistics
    int getDrawCalls() const { return drawCalls; }
    int getVerticesRendered() const { return verticesRendered; }
    void resetStatistics();

    // Immediate mode rendering (for debug/utility)
    void renderLine(const glm::vec3 &start, const glm::vec3 &end,
                    const glm::vec3 &color = glm::vec3(1.0f));
    void renderWireCube(const glm::vec3 &center, const glm::vec3 &size,
                        const glm::vec3 &color = glm::vec3(1.0f));
    void renderLightbulb(const glm::vec3 &center,
                         const glm::vec3 &color = glm::vec3(1.0f));

    int getTextureCounter() { return textureCounter; }

    void renderGameObject(GameObject &gameObject, std::vector<glm::vec3> &lightPos, glm::mat4 lightMatrix);
    void renderGameObjectWithShader(GameObject &gameObject, Shader shader);
    void renderGameObjectWithTexture(GameObject &gameObject, Shader shader, unsigned int textureID);
    void renderGameObjectWithColor(GameObject &gameObject, Shader shader, glm::vec4 color);
    void renderGhostObject(GameObject &gameObject, glm::vec3 position, float alpha = 0.4f);

    void renderCameraAttachedObject(GameObject &gameObject, Shader shader);

    void renderQuadForSmoke();
    void renderQuad();
    void renderCube();
    void renderCube(glm::vec3 position);
    void renderSelectedTile(glm::vec3 position, glm::vec3 color, float tileHeight, float heightFromCube);
    void renderLine(glm::vec3 rayOrigin, glm::vec3 rayDir, glm::mat4 view, float thickness, float length);
    void renderInfiniteGrid(glm::mat4 view, glm::vec3 cameraPosition, Shader shader, float spacing, float fadeDistance,
                            float lineThickness, int visibleRange);
    void renderGridAdvanced(glm::mat4 view, int gridSize, float spacing,
                            float lineThickness, bool drawCenterLines,
                            bool drawYAxis, float yAxisHeight);
    void renderGrid(glm::mat4 view, glm::mat4 projection);
    void renderGrassPoints(const std::vector<glm::vec3> &positions);

    void renderGameObjectWithShader(GameObject &gameObject, Shader shader, glm::mat4 newProjectionMatrix, glm::mat4 newViewMatrix, glm::mat4 newModel);
    void drawShadowCaster(GameObject &gameObject, Shader *shader);

    void initializeShaders();
    void initializeDepthFBO();

    // Load all PBR materials from the serialiser's material library into GPU.
    // Call this once after Scene::buildFromSerializer().
    void loadPBRMaterials(const std::unordered_map<std::string, PBRMaterialDef> &defs);

    void renderSceneToIDBuffer(std::vector<std::shared_ptr<GameObject>> gameObjects);
    unsigned int getObjectId(int mouseX, int mouseY);

    void renderGrass(const glm::vec3 &position, float grassHeight, int grassDensity, float windStrength);
    void renderParabolicTrajectory(glm::vec3 start, glm::vec3 target, int segments);
    void renderLinearTrajectory(glm::vec3 start, glm::vec3 target, int segments);

    void onWindowResize(int newWidth, int newHeight)
    {
        screenWidth = newWidth;
        screenHeight = newHeight;

        std::cout << "Window resized to " << newWidth << "x" << newHeight << std::endl;

        // Recreate ID buffer with new dimensions
        setupIDBuffer();

        std::cout << "ID buffer recreated for new window size" << std::endl;

        // Recreate reflection FBO at new half-resolution
        setupReflectionFBO();
    };

    void renderArrow(glm::vec3 position);
    void renderVerticalArrow(glm::vec3 position);
    void renderEl(glm::vec3 position);
    void renderLine(glm::vec3 position);

    void renderSkyBox();
    void setUpSkyBox();

    // Parallel animation prepare — call once per frame before any render pass.
    // Runs ComputeBoneTransforms for every animated model on background threads,
    // then waits.  All Draw() calls this frame will use the cached results.
    void prepareAllAnimations();

    void renderShadowPass();
    void renderMainPass();

    // Rain system
    void initRainSystem();
    void renderRainPass(float dt);
    void setRainEnabled(bool enabled) { rainEnabled = enabled; }
    bool getRainEnabled() const { return rainEnabled; }

    // Water system
    void generateWaterMesh(float halfExtent = 1500.0f, float yLevel = 0.0f, int divisions = 200);
    void renderWaterPass();

    // Ground system
    void generateGroundMesh(float halfExtent = 200.0f, float yLevel = -2.0f, int divisions = 8);
    void renderGroundPass();

    // Procedural terrain system
    void generateProcTerrainMesh(float halfExtentX, float halfExtentZ, float baseY,
                                 int divisions, float centerX = 0.0f, float centerZ = 0.0f);
    void renderProcTerrainPass();

    // Planar reflection system
    void setupReflectionFBO();
    void renderReflectionPass();

    // Trajectory control methods
    void setRenderTrajectory(bool enabled) { renderTrajectory = enabled; }
    bool getRenderTrajectory() const { return renderTrajectory; }
    void setTrajectorySegments(int segments) { trajectorySegments = segments; }
    int getTrajectorySegments() const { return trajectorySegments; }

    // ID buffer control methods
    void requestIDBufferUpdate() { needIDBufferUpdate = true; }

    std::vector<Light> &getSceneLights()
    {
        return lights;
    };
};