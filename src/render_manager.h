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

// Forward declarations
// class Shader;
// class Texture;
// class Mesh;
// class Camera;

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

struct Light
{
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
    // Add more light properties as needed
};

class RenderManager
{
private:
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

    // G-Buffer members
    unsigned int gBuffer;
    unsigned int gPosition, gNormal, gAlbedoSpec, gDepth, gLinearDepth, gMetallic, gRoughness;
    bool gBufferInitialized;

    // MSAA G-Buffer members
    unsigned int msaaGBuffer;
    unsigned int msaaGPosition, msaaGNormal, msaaGAlbedoSpec, msaaGDepth, msaaGLinearDepth, msaaGMetallic, msaaGRoughness;
    bool msaaGBufferInitialized;
    int msaaSamples;

    // Skybox members
    unsigned int skyboxVAO, skyboxVBO;
    unsigned int skyboxTexture;
    bool skyboxInitialized;
    Shader *skyboxShader;

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

public:
    RenderManager();
    ~RenderManager();

    Camera *currentCamera;

    unsigned int depthFBO;
    unsigned int depthTexture;

    // Initialization
    bool initialize(int width, int height);
    void cleanup();

    void setRes(int height, int width)
    {
        screenWidth = width;
        screenHeight = height;
    };

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
    void useShader(GameObject &gameObject, Shader *shader);
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
    void renderSphere(const glm::vec3 &center, float radius,
                      const glm::vec3 &color = glm::vec3(1.0f));

    int getTextureCounter() { return textureCounter; }

    void renderGameObject(GameObject &gameObject);
    void renderGameObjectWithShader(GameObject &gameObject, Shader shader);
    void renderGameObjectWithTexture(GameObject &gameObject, Shader shader, unsigned int textureID);
    void renderGameObjectWithColor(GameObject &gameObject, Shader shader, glm::vec4 color);

    void renderCameraAttachedObject(GameObject &gameObject, Shader shader);

    void renderQuadForSmoke();
    void renderQuad();
    void renderCube();
    void renderLine(glm::vec3 rayOrigin, glm::vec3 rayDir, glm::mat4 view, float thickness, float length);
    void renderInfiniteGrid(glm::mat4 view, glm::vec3 cameraPosition, Shader shader, float spacing, float fadeDistance,
                            float lineThickness, int visibleRange);
    void renderGridAdvanced(glm::mat4 view, int gridSize, float spacing,
                            float lineThickness, bool drawCenterLines,
                            bool drawYAxis, float yAxisHeight);
    void renderGrid(glm::mat4 view, glm::mat4 projection);
    void renderGrassPoints(const std::vector<glm::vec3> &positions);

    void initializeShaders();
    void initializeDepthFBO();

    void renderSceneToIDBuffer(std::vector<std::shared_ptr<GameObject>> gameObjects);
    unsigned int getObjectId(int mouseX, int mouseY);

    void onWindowResize(int newWidth, int newHeight)
    {
        screenWidth = newWidth;
        screenHeight = newHeight;

        std::cout << "Window resized to " << newWidth << "x" << newHeight << std::endl;

        // Recreate ID buffer with new dimensions
        setupIDBuffer();

        std::cout << "ID buffer recreated for new window size" << std::endl;
    };

private:
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
};