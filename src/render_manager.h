#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../src/camera.h"
#include "../src/mesh.h"
#include "../src/shader_m.h"
#include "../src/texture.h"

// Forward declarations
// class Shader;
// class Texture;
// class Mesh;
// class Camera;

struct RenderCommand {
    Mesh* mesh;
    Shader* shader;
    glm::mat4 modelMatrix;
    std::vector<Texture*> textures;
    float distance; // for sorting
};

struct Light {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
    // Add more light properties as needed
};

class RenderManager {
private:
    // Rendering queues
    std::vector<RenderCommand> opaqueQueue;
    std::vector<RenderCommand> transparentQueue;
    std::vector<RenderCommand> uiQueue;
    
    // Shader management
    // std::unordered_map<std::string, std::unique_ptr<Shader>> shaders;
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

public:
    RenderManager();
    ~RenderManager();

    Camera* currentCamera;
    
    // Initialization
    bool initialize(int width, int height);
    void cleanup();
    
    // Frame management
    void beginFrame();
    void endFrame();
    void clear();
    void present();
    
    // Render queue management
    void submit(Mesh* mesh, Shader* shader, const glm::mat4& modelMatrix, 
                const std::vector<Texture*>& textures = {});
    void submitUI(Mesh* mesh, Shader* shader, const glm::mat4& modelMatrix);
    void submitTransparent(Mesh* mesh, Shader* shader, const glm::mat4& modelMatrix, 
                          const std::vector<Texture*>& textures = {});
    
    // Rendering
    void renderScene();
    void renderOpaqueObjects();
    void renderTransparentObjects();
    void renderUI();
    
    // Resource management
    Shader* loadShader(const std::string& name, const std::string& vertexPath, 
                      const std::string& fragmentPath);
    unsigned int loadTexture(const std::string& name, const char* path);
    Mesh* loadMesh(const std::string& name, const std::string& path);
    
    Shader* getShader(const std::string& name);
    Texture* getTexture(const std::string& name);
    Mesh* getMesh(const std::string& name);
    
    // Camera management
    void setCamera(Camera* camera);
    void updateCameraMatrices();
    
    // Lighting
    void addLight(const Light& light);
    void clearLights();
    void setAmbientLight(const glm::vec3& color);
    void updateLightUniforms(Shader* shader);
    
    // Render state management
    void setClearColor(const glm::vec4& color);
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
    void renderLine(const glm::vec3& start, const glm::vec3& end, 
                   const glm::vec3& color = glm::vec3(1.0f));
    void renderWireCube(const glm::vec3& center, const glm::vec3& size, 
                       const glm::vec3& color = glm::vec3(1.0f));
    void renderSphere(const glm::vec3& center, float radius, 
                     const glm::vec3& color = glm::vec3(1.0f));

    

private:
    // Internal helper functions
    void bindTextures(const std::vector<Texture*>& textures);
    void unbindTextures();
    void setupShaderUniforms(Shader* shader, const glm::mat4& modelMatrix);
    float calculateDistance(const glm::vec3& position);
};