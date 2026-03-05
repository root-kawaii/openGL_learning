#include "render_manager.h"
#include "game.h"
#include "ui.h"
#include "serialization_utilities.h"
#include "../tracy/public/tracy/Tracy.hpp"
#include <future>
#include <unordered_set>
#include <../include/stb_image.h>

namespace fs = std::filesystem;

bool isInFrustum(const glm::vec3 &position, float radius,
                 const glm::mat4 &viewProjection)
{
    // Transform object position to clip space
    glm::vec4 clipSpacePos = viewProjection * glm::vec4(position, 1.0f);

    // Perspective divide
    if (clipSpacePos.w != 0.0f)
    {
        clipSpacePos /= clipSpacePos.w;
    }

    // Add radius in clip space (approximate)
    float radiusInClipSpace = radius / clipSpacePos.w;

    // Check if within normalized device coordinates [-1, 1] with radius
    if (clipSpacePos.x < -1.0f - radiusInClipSpace || clipSpacePos.x > 1.0f + radiusInClipSpace)
        return false;
    if (clipSpacePos.y < -1.0f - radiusInClipSpace || clipSpacePos.y > 1.0f + radiusInClipSpace)
        return false;
    if (clipSpacePos.z < -1.0f - radiusInClipSpace || clipSpacePos.z > 1.0f + radiusInClipSpace)
        return false;

    return true;
}

bool isInViewDistance(const glm::vec3 &objectPos, const glm::vec3 &cameraPos, float maxDistance)
{
    float distSq = glm::length(objectPos - cameraPos);
    distSq = distSq * distSq;
    return distSq < (maxDistance * maxDistance);
}

// Light-space frustum culling for shadow pass optimization
bool isInLightFrustum(const glm::vec3 &position, float radius,
                      const glm::mat4 &lightProjection, const glm::mat4 &lightView)
{
    glm::mat4 lightVP = lightProjection * lightView;
    glm::vec4 clipSpacePos = lightVP * glm::vec4(position, 1.0f);

    // Perspective divide
    if (clipSpacePos.w != 0.0f)
    {
        clipSpacePos /= clipSpacePos.w;
    }

    // Add radius in clip space (approximate)
    float radiusInClipSpace = radius / abs(clipSpacePos.w);

    // Check if within normalized device coordinates [-1, 1] with radius
    if (clipSpacePos.x < -1.0f - radiusInClipSpace || clipSpacePos.x > 1.0f + radiusInClipSpace)
        return false;
    if (clipSpacePos.y < -1.0f - radiusInClipSpace || clipSpacePos.y > 1.0f + radiusInClipSpace)
        return false;
    if (clipSpacePos.z < -1.0f - radiusInClipSpace || clipSpacePos.z > 1.0f + radiusInClipSpace)
        return false;

    return true;
}

float skyboxVertices[] = {
    // positions
    -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f,

    -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
    -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f,

    1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,

    -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

unsigned int loadCubemapForSkyBox(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data =
            stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            // Keep GL_RGB (not GL_SRGB): the skybox display shader passes raw texel values
            // straight to the framebuffer, so they must stay in sRGB encoding for correct
            // monitor output. PBR shaders linearise explicitly with pow(envColor, 2.2).
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height,
                         0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap tex failed to load at path: " << faces[i]
                      << std::endl;
            stbi_image_free(data);
        }
    }
    // Generate mipmaps so textureLod() in PBR shaders can sample blurred
    // reflections for rough surfaces (higher lod = blurrier = rougher look).
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

RenderManager::RenderManager()
    : currentCamera(nullptr), clearColor(0.2f, 0.3f, 0.3f, 1.0f), wireframeMode(false), depthTestEnabled(true), blendingEnabled(false), screenWidth(800), screenHeight(600), drawCalls(0), verticesRendered(0), ambientLight(0.1f, 0.1f, 0.1f), boneDebugMode(0)
{
    textureCounter = 0;
}

RenderManager::~RenderManager()
{
    cleanup();
}

bool RenderManager::initialize(int width, int height)
{
    // TODO: Initialize OpenGL states, default shaders, etc.
    setRes(width, height);
    setupIDBuffer();
    arrowModel = std::make_shared<Model>("assets/arrow.obj");
    lineModel = std::make_shared<Model>("assets/line.obj");
    elModel = std::make_shared<Model>("assets/el.obj");
    torchModel = std::make_shared<Model>("assets/torch.glb");
    setUpSkyBox();
    // 1500 half-extent covers the full 1000-unit far plane in all directions.
    // Y=-0.5 keeps the water below the tile floor (tiles have centers at Y~0.5)
    // so the level sits as an island above the water surface.
    generateWaterMesh(1500.0f, -0.5f, 200);
    // Ground plane: 2 units below main level floor (tile bottom = Y 0, so -2.0).
    // 200 half-extent spans the visible seabed under the transparent water.
    generateGroundMesh(200.0f, -2.0f, 8);
    // Procedural terrain: covers the level footprint (150-unit half-extent),
    // base at Y=-1 so it sits just below the tile floor, FBM amplitude +10 upwards.
    generateProcTerrainMesh(5.0f, 5.0f, -1.0f, 2500, 0.0f, 0.0f);
    setupReflectionFBO();
    return true;
}

void RenderManager::cleanup()
{
    // TODO: Clean up resources
}

void RenderManager::setLights(const std::vector<Light> &sceneLights)
{
    // Clear existing lights
    lightPositions.clear();
    lights.clear();

    // Convert SceneLight to Light and lightPositions
    for (const auto &sceneLight : sceneLights)
    {
        // Add to lightPositions for backward compatibility
        lightPositions.push_back(sceneLight.position);

        // Create Light struct
        Light light;
        light.position = sceneLight.position;
        light.color = sceneLight.color;
        light.intensity = sceneLight.intensity;
        lights.push_back(light);
    }

    std::cout << "RenderManager: Loaded " << lights.size() << " lights from scene" << std::endl;
}

void RenderManager::beginFrame()
{
    // TODO: Reset frame data, clear queues
    opaqueQueue.clear();
    transparentQueue.clear();
    uiQueue.clear();
    resetStatistics();
}

void RenderManager::endFrame()
{
    // TODO: Final frame operations
}

void RenderManager::clear()
{
    // TODO: Clear color and depth buffers
}

void RenderManager::present()
{
    // TODO: Swap buffers or present frame
}

void RenderManager::checkAndReloadShaders()
{
    if (lastTimeSinceShaderReload < 1.0f)
        return;

    // Reset timer
    lastTimeSinceShaderReload = 0.0f;

    // std::cout << "Checking shaders for modifications..." << std::endl;

    // Check each shader for file modifications
    for (auto &[shaderName, shader] : shaders)
    {
        if (!shader)
            continue;

        try
        {
            auto vertexPath = shader->getVertexPath();
            auto fragmentPath = shader->getFragmentPath();

            // Check if shader files exist
            if (!fs::exists(vertexPath) || !fs::exists(fragmentPath))
                continue;

            // Get file modification times
            auto vertexTime = fs::last_write_time(vertexPath);
            auto fragmentTime = fs::last_write_time(fragmentPath);
            auto now = fs::file_time_type::clock::now();

            // Check if modified in last 2 seconds
            bool vertexModified = (now - vertexTime) < std::chrono::seconds(2);
            bool fragmentModified = (now - fragmentTime) < std::chrono::seconds(2);

            if (vertexModified || fragmentModified)
            {
                std::cout << "Reloading shader: " << shaderName << std::endl;

                // Reload the shader
                shaders[shaderName] = std::make_shared<Shader>(
                    vertexPath.c_str(),
                    fragmentPath.c_str(),
                    shader->hasGeometryShader() ? shader->getGeometryPath().c_str() : nullptr);
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            // Silently ignore filesystem errors
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error reloading shader " << shaderName << ": " << e.what() << std::endl;
        }
    }
}

void RenderManager::submit(Mesh *mesh, Shader *shader, const glm::mat4 &modelMatrix, const std::vector<Texture *> &textures)
{
    // TODO: Add to opaque queue
}

void RenderManager::submitUI(Mesh *mesh, Shader *shader, const glm::mat4 &modelMatrix)
{
    // TODO: Add to UI queue
}

void RenderManager::submitTransparent(Mesh *mesh, Shader *shader, const glm::mat4 &modelMatrix, const std::vector<Texture *> &textures)
{
    // TODO: Add to transparent queue
}

void RenderManager::renderScene()
{
    // TODO: Render all queues in order
    renderOpaqueObjects();
    renderTransparentObjects();
    renderUI();
}

void RenderManager::renderOpaqueObjects()
{
    // TODO: Render opaque queue
}

void RenderManager::renderTransparentObjects()
{
    // TODO: Sort and render transparent queue
}

void RenderManager::renderUI()
{
    // TODO: Render UI queue
}

Shader *RenderManager::loadShader(const std::string &name, const std::string &vertexPath, const std::string &fragmentPath)
{
    // TODO: Load and compile shader
    return nullptr;
}

unsigned int RenderManager::loadTexture(const std::string &name, const char *path)
{
    ZoneScoped;
    auto it = textures.find(name);
    if (it != textures.end())
    {
        return it->second->id;
    }

    auto tex = std::make_shared<Texture>(path);

    textures.emplace(name, tex);
    return tex->id;
}

Mesh *RenderManager::loadMesh(const std::string &name, const std::string &path)
{
    // TODO: Load mesh from file
    return nullptr;
}

Shader *RenderManager::getShader(const std::string &name)
{
    if (name == "default")
        return shaders.at("simple_shader").get();
    return shaders.at(name).get();
}

Texture *RenderManager::getTexture(const std::string &name)
{
    // TODO: Return texture by name
    return nullptr;
}

Mesh *RenderManager::getMesh(const std::string &name)
{
    // TODO: Return mesh by name
    return nullptr;
}

void RenderManager::setCamera(Camera *camera)
{
    // TODO: Set current camera
    currentCamera = camera;
}

void RenderManager::updateCameraMatrices()
{
    // TODO: Update view and projection matrices from camera
}

void RenderManager::addLight(const Light &light)
{
    // TODO: Add light to lights vector
}

void RenderManager::clearLights()
{
    // TODO: Clear all lights
    lights.clear();
}

void RenderManager::setAmbientLight(const glm::vec3 &color)
{
    // TODO: Set ambient light color
    ambientLight = color;
}

void RenderManager::updateLightUniforms(Shader *shader)
{
    // TODO: Update light uniforms in shader
}

void RenderManager::setClearColor(const glm::vec4 &color)
{
    // TODO: Set clear color
    clearColor = color;
}

void RenderManager::setWireframeMode(bool enabled)
{
    // TODO: Enable/disable wireframe mode
    wireframeMode = enabled;
}

void RenderManager::enableDepthTest(bool enabled)
{
    // TODO: Enable/disable depth testing
    depthTestEnabled = enabled;
}

void RenderManager::enableBlending(bool enabled)
{
    // TODO: Enable/disable blending
    blendingEnabled = enabled;
}

void RenderManager::setViewport(int width, int height)
{
    // TODO: Set OpenGL viewport
    screenWidth = width;
    screenHeight = height;
}

void RenderManager::sortTransparentQueue()
{
    // TODO: Sort transparent objects by distance from camera
}

void RenderManager::setupRenderStates()
{
    // TODO: Setup OpenGL render states
}

void RenderManager::resetRenderStates()
{
    // TODO: Reset OpenGL render states to default
}

void RenderManager::resetStatistics()
{
    // TODO: Reset frame statistics
    drawCalls = 0;
    verticesRendered = 0;
}

void RenderManager::renderLine(const glm::vec3 &start, const glm::vec3 &end, const glm::vec3 &color)
{
    // TODO: Immediate mode line rendering
}

void RenderManager::renderWireCube(const glm::vec3 &center, const glm::vec3 &size, const glm::vec3 &color)
{
    // TODO: Immediate mode wire cube rendering
}

void RenderManager::renderLightbulb(const glm::vec3 &center, const glm::vec3 &color)
{
    static unsigned int bulbVAO = 0;
    static unsigned int bulbVBO = 0;
    static int vertexCount = 0;

    if (bulbVAO == 0)
    {
        std::vector<float> vertices;
        const unsigned int X_SEGMENTS = 20;
        const unsigned int Y_SEGMENTS = 20;
        const float RADIUS = 0.2f; // Small size for a bulb

        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
        {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * 3) * std::sin(ySegment * 3);
                float yPos = std::cos(ySegment * 3);
                float zPos = std::sin(xSegment * 2.0f * 3) * std::sin(ySegment * 3);

                // Position
                vertices.push_back(xPos * RADIUS);
                vertices.push_back(yPos * RADIUS);
                vertices.push_back(zPos * RADIUS);
                // Normals (same as position for a sphere)
                vertices.push_back(xPos);
                vertices.push_back(yPos);
                vertices.push_back(zPos);
            }
        }

        // Generate indices for a triangle strip or convert to triangles
        // For simplicity and matching your glDrawArrays style,
        // we can use a basic sphere generation algorithm here.
        // (Note: In a production app, use an EBO/Index Buffer)

        glGenVertexArrays(1, &bulbVAO);
        glGenBuffers(1, &bulbVBO);
        glBindVertexArray(bulbVAO);
        glBindBuffer(GL_ARRAY_BUFFER, bulbVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        glEnableVertexAttribArray(0); // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(1); // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));

        vertexCount = vertices.size() / 6;
    }

    // 1. Set your shader to a simple "Unlit" or "Flat" shader
    // lightShader.use();
    // lightShader.setVec3("lightColor", color);

    // 2. Set Model Matrix
    glm::mat4 model = glm::mat4(100.0f);
    model = glm::translate(model, center);
    // lightShader.setMat4("model", model);

    // 3. Render
    glBindVertexArray(bulbVAO);
    // Note: This uses GL_POINTS for a 'star' effect or GL_TRIANGLE_STRIP
    // depending on how you structured the vertex generation above.
    glDrawArrays(GL_POINTS, 0, vertexCount);
    glBindVertexArray(0);
}

void RenderManager::bindTextures(const std::vector<Texture *> &textures)
{
    // TODO: Bind textures to texture units
}

void RenderManager::unbindTextures()
{
    // TODO: Unbind all textures
}

void RenderManager::setupShaderUniforms(Shader *shader, const glm::mat4 &modelMatrix)
{
    // TODO: Set common shader uniforms (MVP matrices, lights, etc.)
}

float RenderManager::calculateDistance(const glm::vec3 &position)
{
    // TODO: Calculate distance from camera position
    return 0.0f;
}

void RenderManager::setupGBuffer()
{
    ZoneScoped;
    if (gBufferInitialized)
    {
        cleanupGBuffer(); // Clean up existing G-Buffer first
    }

    // Generate framebuffer
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    createGBufferTextures();

    // Tell OpenGL which color attachments we'll use for rendering
    unsigned int attachments[6] = {
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5};
    glDrawBuffers(6, attachments);

    // Check framebuffer completeness
    if (!checkGBufferStatus())
    {
        std::cerr << "ERROR::FRAMEBUFFER:: G-buffer is not complete!" << std::endl;
        return;
    }

    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gBufferInitialized = true;

    std::cout << "G-Buffer setup complete!" << std::endl;
}

void RenderManager::createGBufferTextures()
{
    ZoneScoped;
    // Position color buffer
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    // Normal color buffer
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    // Color + specular color buffer
    glGenTextures(1, &gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

    // Depth buffer
    glGenTextures(1, &gDepth);
    glBindTexture(GL_TEXTURE_2D, gDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, screenWidth, screenHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gDepth, 0);

    // Linear Depth buffer
    glGenTextures(1, &gLinearDepth);
    glBindTexture(GL_TEXTURE_2D, gLinearDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, screenWidth, screenHeight, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gLinearDepth, 0);

    // Metallic
    glGenTextures(1, &gMetallic);
    glBindTexture(GL_TEXTURE_2D, gMetallic);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gMetallic, 0);

    // Roughness
    glGenTextures(1, &gRoughness);
    glBindTexture(GL_TEXTURE_2D, gRoughness);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, gRoughness, 0);
}

bool RenderManager::checkGBufferStatus()
{
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        switch (status)
        {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT" << std::endl;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT" << std::endl;
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            std::cerr << "GL_FRAMEBUFFER_UNSUPPORTED" << std::endl;
            break;
        default:
            std::cerr << "Unknown framebuffer error: " << status << std::endl;
            break;
        }
        return false;
    }
    return true;
}

void RenderManager::cleanupGBuffer()
{
    if (gBufferInitialized)
    {
        glDeleteTextures(1, &gPosition);
        glDeleteTextures(1, &gNormal);
        glDeleteTextures(1, &gAlbedoSpec);
        glDeleteTextures(1, &gDepth);
        glDeleteTextures(1, &gLinearDepth);
        glDeleteTextures(1, &gMetallic);
        glDeleteTextures(1, &gRoughness);
        glDeleteFramebuffers(1, &gBuffer);
        gBufferInitialized = false;
    }
}

void RenderManager::bindGBuffer()
{
    if (gBufferInitialized)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
        glViewport(0, 0, screenWidth, screenHeight);
    }
}

void RenderManager::unbindGBuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderManager::resizeGBuffer(int width, int height)
{
    if (width != screenWidth || height != screenHeight)
    {
        screenWidth = width;
        screenHeight = height;
        if (gBufferInitialized)
        {
            setupGBuffer(); // Recreate G-Buffer with new dimensions
        }
    }
}

void RenderManager::setupMSAAGBuffer(int samples)
{
    ZoneScoped;
    if (msaaGBufferInitialized)
    {
        cleanupMSAAGBuffer(); // Clean up existing MSAA G-Buffer first
    }

    msaaSamples = samples;

    // Generate framebuffer
    glGenFramebuffers(1, &msaaGBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaGBuffer);

    createMSAAGBufferTextures();

    // Set draw buffers for MSAA G-buffer
    unsigned int msaaAttachments[6] = {
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5};
    glDrawBuffers(6, msaaAttachments);

    // Check framebuffer completeness
    if (!checkMSAAGBufferStatus())
    {
        std::cerr << "ERROR::FRAMEBUFFER:: MSAA G-buffer is not complete!" << std::endl;
        return;
    }

    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    msaaGBufferInitialized = true;

    std::cout << "MSAA G-Buffer setup complete with " << samples << " samples!" << std::endl;
}

void RenderManager::createMSAAGBufferTextures()
{
    ZoneScoped;
    // Position buffer (MSAA)
    glGenTextures(1, &msaaGPosition);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaGPosition);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaaSamples, GL_RGBA16F, screenWidth, screenHeight, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, msaaGPosition, 0);

    // Normal buffer (MSAA)
    glGenTextures(1, &msaaGNormal);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaGNormal);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaaSamples, GL_RGBA16F, screenWidth, screenHeight, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D_MULTISAMPLE, msaaGNormal, 0);

    // Albedo + Specular buffer (MSAA)
    glGenTextures(1, &msaaGAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaGAlbedoSpec);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaaSamples, GL_RGBA8, screenWidth, screenHeight, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D_MULTISAMPLE, msaaGAlbedoSpec, 0);

    // Depth buffer (MSAA) - using renderbuffer for depth
    glGenRenderbuffers(1, &msaaGDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaGDepth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamples, GL_DEPTH_COMPONENT24, screenWidth, screenHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaGDepth);

    // Linear Depth buffer (MSAA)
    glGenTextures(1, &msaaGLinearDepth);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaGLinearDepth);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaaSamples, GL_R32F, screenWidth, screenHeight, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D_MULTISAMPLE, msaaGLinearDepth, 0);

    // Metallic buffer (MSAA)
    glGenTextures(1, &msaaGMetallic);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaGMetallic);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaaSamples, GL_RGBA8, screenWidth, screenHeight, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D_MULTISAMPLE, msaaGMetallic, 0);

    // Roughness buffer (MSAA)
    glGenTextures(1, &msaaGRoughness);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaGRoughness);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaaSamples, GL_RGBA8, screenWidth, screenHeight, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D_MULTISAMPLE, msaaGRoughness, 0);
}

bool RenderManager::checkMSAAGBufferStatus()
{
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        switch (status)
        {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            std::cerr << "MSAA GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT" << std::endl;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            std::cerr << "MSAA GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT" << std::endl;
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            std::cerr << "MSAA GL_FRAMEBUFFER_UNSUPPORTED" << std::endl;
            break;
        default:
            std::cerr << "Unknown MSAA framebuffer error: " << status << std::endl;
            break;
        }
        return false;
    }
    return true;
}

void RenderManager::cleanupMSAAGBuffer()
{
    if (msaaGBufferInitialized)
    {
        glDeleteTextures(1, &msaaGPosition);
        glDeleteTextures(1, &msaaGNormal);
        glDeleteTextures(1, &msaaGAlbedoSpec);
        glDeleteRenderbuffers(1, &msaaGDepth); // Note: renderbuffer for depth
        glDeleteTextures(1, &msaaGLinearDepth);
        glDeleteTextures(1, &msaaGMetallic);
        glDeleteTextures(1, &msaaGRoughness);
        glDeleteFramebuffers(1, &msaaGBuffer);
        msaaGBufferInitialized = false;
    }
}

void RenderManager::bindMSAAGBuffer()
{
    if (msaaGBufferInitialized)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, msaaGBuffer);
        glViewport(0, 0, screenWidth, screenHeight);
    }
}

void RenderManager::unbindMSAAGBuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderManager::resolveMSAAGBuffer()
{
    ZoneScoped;
    if (!msaaGBufferInitialized || !gBufferInitialized)
    {
        std::cerr << "ERROR: Both MSAA and regular G-Buffers must be initialized for resolving!" << std::endl;
        return;
    }

    // Resolve each MSAA texture to regular texture
    // Note: You need to resolve each attachment individually

    // Resolve position
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaGBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gBuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Resolve normal
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Resolve albedo
    glReadBuffer(GL_COLOR_ATTACHMENT2);
    glDrawBuffer(GL_COLOR_ATTACHMENT2);
    glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Resolve linear depth
    glReadBuffer(GL_COLOR_ATTACHMENT3);
    glDrawBuffer(GL_COLOR_ATTACHMENT3);
    glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Resolve metallic
    glReadBuffer(GL_COLOR_ATTACHMENT4);
    glDrawBuffer(GL_COLOR_ATTACHMENT4);
    glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Resolve roughness
    glReadBuffer(GL_COLOR_ATTACHMENT5);
    glDrawBuffer(GL_COLOR_ATTACHMENT5);
    glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Resolve depth
    glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderManager::resizeMSAAGBuffer(int width, int height)
{
    if (width != screenWidth || height != screenHeight)
    {
        screenWidth = width;
        screenHeight = height;
        if (msaaGBufferInitialized)
        {
            setupMSAAGBuffer(msaaSamples); // Recreate MSAA G-Buffer with new dimensions
        }
    }
}

// Load cubemap texture
unsigned int RenderManager::loadCubemap(const std::vector<std::string> &faces)
{
    textures.at(0)->loadCubemap(faces);
}

// // Update renderScene() to include skybox rendering
// void RenderManager::renderScene()
// {
//     // Render opaque objects first
//     renderOpaqueObjects();

//     // Render skybox (after opaque, before transparent)
//     renderSkybox();

//     // Render transparent objects
//     renderTransparentObjects();

//     // Render UI last
//     renderUI();
// }

// // Update updateCameraMatrices() to store projection matrix
// void RenderManager::updateCameraMatrices()
// {
//     if (currentCamera) {
//         viewMatrix = currentCamera->GetViewMatrix();
//         projectionMatrix = currentCamera->GetProjectionMatrix();
//     }
// }

void RenderManager::renderCube()
{
    ZoneScoped;
    static unsigned int cubeVAO = 0;
    static unsigned int cubeVBO = 0;
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back facer
            -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
            1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,  // bottom-right
            1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
            -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,  // top-left
            // front face
            -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
            1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right
            1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
            1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
            -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // top-left
            -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
            -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // top-left
            -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
            -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
                                                                // right face
            1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left
            1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right
            1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,    // top-right
            1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right
            1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left
            1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,    // bottom-left
            // bottom face
            -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
            1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,  // top-left
            1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
            1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
            -1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
            -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
            1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
            1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // top-right
            1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
            -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
            -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f   // bottom-left
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void RenderManager::renderSelectedTile(glm::vec3 position, glm::vec3 color, float selectionHeight, float heightFromCube)
{
    position = position + glm::vec3(0, heightFromCube, 0);
    Shader *simpleShader = getShader("tile_shader");
    simpleShader->use();

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Optional: disable depth writing so objects behind show through
    // glDepthMask(GL_FALSE);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::scale(model, glm::vec3(0.5f, selectionHeight, 0.5f));
    simpleShader->setMat4("model", model);
    simpleShader->setMat4("view", viewMatrix);
    simpleShader->setMat4("projection", projectionMatrix);
    simpleShader->setVec3("baseColor", color);
    simpleShader->setFloat("alpha", 0.5f); // Add alpha uniform (50% transparent)
    simpleShader->setFloat("time", glfwGetTime());

    renderCube(position);

    // Restore OpenGL state
    // glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void RenderManager::renderCube(glm::vec3 position)
{
    ZoneScoped;

    position.x = std::floor(position.x) + 0.5f;
    // position.y = std::floor(position.y) + 0.5f;
    position.z = std::floor(position.z) + 0.5f;

    static unsigned int cubeVAO = 0;
    static unsigned int cubeVBO = 0;
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
            1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,  // bottom-right
            1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
            -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,  // top-left
            // front face
            -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
            1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right
            1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
            1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
            -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // top-left
            -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
            -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // top-left
            -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
            -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
            // right face
            1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-left
            1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-right
            1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // top-right
            1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-right
            1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-left
            1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-left
            // bottom face
            -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
            1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,  // top-left
            1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
            1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
            -1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
            -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
            1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
            1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // top-right
            1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
            -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
            -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f   // bottom-left
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // Set the model matrix uniform in your shader
    // Assuming you have a shader program bound and it has a "model" uniform

    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void RenderManager::renderLine(glm::vec3 rayOrigin, glm::vec3 rayDir, glm::mat4 view, float thickness = 0.1f, float length = 0.1f)
{
    ZoneScoped;
    static unsigned int lineVAO = 0; // Add 'static'
    static unsigned int lineVBO = 0; // Add 'static'
    if (lineVAO == 0)
    {
        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);
    }

    // Normalize the ray direction
    rayDir = glm::normalize(rayDir);

    // Compute the end point along the ray
    glm::vec3 rayEnd = rayOrigin + rayDir * length;
    // glm::vec3 renderRayStart = rayOrigin + glm::vec3(rayDir);

    // Get camera vectors from view matrix
    glm::vec3 camRight = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
    glm::vec3 camUp = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));

    // Direction of the line
    glm::vec3 lineDir = rayDir;

    // Compute two perpendicular vectors for thickness in both directions
    glm::vec3 offset1 = glm::normalize(glm::cross(lineDir, camRight)) * (thickness * 0.5f);
    glm::vec3 offset2 = glm::normalize(glm::cross(lineDir, offset1)) * (thickness * 0.5f);

    // Check for degenerate cases
    if (glm::length(offset1) < 1e-6f || glm::length(offset2) < 1e-6f)
    {
        // Fallback: use camera up vector if cross product fails
        offset1 = camRight * (thickness * 0.5f);
        offset2 = camUp * (thickness * 0.5f);
    }

    // Create 8 vertices for a rectangular tube (4 at start, 4 at end)
    glm::vec3 startVerts[4] = {
        rayOrigin + offset1 + offset2, // top-right
        rayOrigin - offset1 + offset2, // top-left
        rayOrigin - offset1 - offset2, // bottom-left
        rayOrigin + offset1 - offset2  // bottom-right
    };

    glm::vec3 endVerts[4] = {
        rayEnd + offset1 + offset2, // top-right
        rayEnd - offset1 + offset2, // top-left
        rayEnd - offset1 - offset2, // bottom-left
        rayEnd + offset1 - offset2  // bottom-right
    };

    // Create vertices for 4 faces (12 triangles total)
    float vertices[] = {
        // Face 1: top (0-1-5-4)
        startVerts[0].x, startVerts[0].y, startVerts[0].z,
        startVerts[1].x, startVerts[1].y, startVerts[1].z,
        endVerts[1].x, endVerts[1].y, endVerts[1].z,

        endVerts[1].x, endVerts[1].y, endVerts[1].z,
        endVerts[0].x, endVerts[0].y, endVerts[0].z,
        startVerts[0].x, startVerts[0].y, startVerts[0].z,

        // Face 2: right (0-4-7-3)
        startVerts[0].x, startVerts[0].y, startVerts[0].z,
        endVerts[0].x, endVerts[0].y, endVerts[0].z,
        endVerts[3].x, endVerts[3].y, endVerts[3].z,

        endVerts[3].x, endVerts[3].y, endVerts[3].z,
        startVerts[3].x, startVerts[3].y, startVerts[3].z,
        startVerts[0].x, startVerts[0].y, startVerts[0].z,

        // Face 3: bottom (3-7-6-2)
        startVerts[3].x, startVerts[3].y, startVerts[3].z,
        endVerts[3].x, endVerts[3].y, endVerts[3].z,
        endVerts[2].x, endVerts[2].y, endVerts[2].z,

        endVerts[2].x, endVerts[2].y, endVerts[2].z,
        startVerts[2].x, startVerts[2].y, startVerts[2].z,
        startVerts[3].x, startVerts[3].y, startVerts[3].z,

        // Face 4: left (2-6-5-1)
        startVerts[2].x, startVerts[2].y, startVerts[2].z,
        endVerts[2].x, endVerts[2].y, endVerts[2].z,
        endVerts[1].x, endVerts[1].y, endVerts[1].z,

        endVerts[1].x, endVerts[1].y, endVerts[1].z,
        startVerts[1].x, startVerts[1].y, startVerts[1].z,
        startVerts[2].x, startVerts[2].y, startVerts[2].z};

    // Upload vertex data
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    // Setup vertex attributes (location 0 = vec3 position)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    glDisable(GL_CULL_FACE);

    // Render the rectangular tube (24 vertices = 8 triangles)
    glDrawArrays(GL_TRIANGLES, 0, 24);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
}

void RenderManager::renderQuad()
{
    ZoneScoped;
    static unsigned int quadVAO = 0;
    static unsigned int quadVBO;
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            0.0f,
            1.0f,
            1.0f,
            1.0f,
            -1.0f,
            0.0f,
            1.0f,
            0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void RenderManager::renderQuadForSmoke()
{
    ZoneScoped;
    static unsigned int quadVAO = 0;
    static unsigned int quadVBO;
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -0.5f,
            0.5f,
            0.0f,
            0.0f,
            1.0f,
            -0.5f,
            -0.5f,
            0.0f,
            0.0f,
            0.0f,
            0.5f,
            0.5f,
            0.0f,
            1.0f,
            1.0f,
            0.5f,
            -0.5f,
            0.0f,
            1.0f,
            0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void RenderManager::renderGameObjectWithShader(GameObject &gameObject, Shader shader)
{
    ZoneScoped;
    // Fast distance check first
    if (!isInViewDistance(gameObject.position, currentCamera->Position, 1000.0f))
    {
        return;
    }

    // Then frustum check
    if (!isInFrustum(gameObject.position, 1.0f, projectionMatrix * viewMatrix))
    {
        return;
    }

    shader.use();
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 scaling = glm::scale(glm::mat4(1.0f), gameObject.scale);
    model = glm::translate(model, gameObject.position) * scaling;
    shader.setMat4("projection", projectionMatrix);
    shader.setMat4("view", viewMatrix);
    shader.setMat4("model", model);
    // shader.setFloat("time", glfwGetTime());
    shader.setFloat("windAngle", 0.785f); // 45 degrees
    shader.setVec3("sandColor", glm::vec3(0.76, 0.7, 0.5));
    shader.setVec3("fogColor", glm::vec3(0.76, 0.7, 0.5));
    shader.setFloat("fogDensity", 0.0001f); // 45 degrees
    gameObject.model->Draw(shader);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in renderGameObjectWithShader 2: " << error << std::endl;
    }
}

void RenderManager::renderGameObjectWithShader(GameObject &gameObject, Shader shader, glm::mat4 newProjectionMatrix, glm::mat4 newViewMatrix, glm::mat4 newModel)
{
    ZoneScoped;
    // Fast distance check first
    if (!isInViewDistance(gameObject.position, currentCamera->Position, 1000.0f))
    {
        return;
    }

    // Then frustum check
    if (!isInFrustum(gameObject.position, 1.0f, projectionMatrix * viewMatrix))
    {
        return;
    }
    shader.use();
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 scaling = glm::scale(glm::mat4(1.0f), gameObject.scale);
    model = glm::translate(model, gameObject.position) * scaling;
    shader.setMat4("projection", newProjectionMatrix);
    shader.setMat4("view", newViewMatrix);
    shader.setMat4("model", model);
    shader.setFloat("time", glfwGetTime());
    gameObject.model->Draw(shader);
}

void RenderManager::drawShadowCaster(GameObject &gameObject, Shader *shader)
{
    // No camera-frustum check here — the shadow pass must render objects that are
    // outside the camera view but still in the light's path (they cast shadows on
    // things the camera CAN see). Culling by camera frustum is what causes missing
    // shadows when an occluder is behind or beside the camera. Only light-space
    // culling (done by the caller) is correct here.
    // Only "model" is needed — lightSpaceMatrix is already set once before the loop.
    shader->setMat4("model", gameObject.GetTransform());
    gameObject.model->Draw(*shader);
}

void RenderManager::renderGameObject(GameObject &gameObject, std::vector<glm::vec3> &lightPos, glm::mat4 lightMatrix)
{
    ZoneScoped;
    ZoneText(gameObject.name.c_str(), gameObject.name.size());

    {
        ZoneScopedN("CullChecks");
        if (!isInViewDistance(gameObject.position, currentCamera->Position, 1000.0f))
            return;
        if (!isInFrustum(gameObject.position, 1.0f, projectionMatrix * viewMatrix))
            return;
    }

    {
        ZoneScopedN("UseShader");
        Shader *shader = getShader(gameObject.shaderName);
        useShader(gameObject, shader, lightPos, lightMatrix);

        if (gameObject.shaderName != "pbr_model_textured")
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, depthTexture);
        }

        {
            ZoneScopedN("Model::Draw");
            gameObject.model->Draw(*shader);
        }
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void RenderManager::setupIDBuffer()
{
    // Clean up existing framebuffer if it exists
    if (IDFrameBuffer != 0)
    {
        std::cout << "Cleaning up existing ID buffer..." << std::endl;
        glDeleteFramebuffers(1, &IDFrameBuffer);
        IDFrameBuffer = 0;
    }
    if (idTexture != 0)
    {
        glDeleteTextures(1, &idTexture);
        idTexture = 0;
    }
    if (rboDepth != 0)
    {
        glDeleteRenderbuffers(1, &rboDepth);
        rboDepth = 0;
    }

    std::cout << "Creating ID buffer: " << screenWidth << "x" << screenHeight << std::endl;

    // Store current dimensions
    idBufferWidth = screenWidth;
    idBufferHeight = screenHeight;

    // 1. Generate and bind the ID framebuffer
    glGenFramebuffers(1, &IDFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, IDFrameBuffer);

    // 2. Create a texture attachment for storing IDs
    glGenTextures(1, &idTexture);
    glBindTexture(GL_TEXTURE_2D, idTexture);

    // Create texture with current screen dimensions
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, screenWidth, screenHeight, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, idTexture, 0);

    // 3. Create a depth renderbuffer attachment
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, screenWidth, screenHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    // 4. Check if the framebuffer is complete
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "ID Framebuffer is not complete! Status: " << status << std::endl;
        return;
    }

    // 5. Unbind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    std::cout << "ID Framebuffer created successfully: " << screenWidth << "x" << screenHeight << std::endl;
}

void RenderManager::renderSceneToIDBuffer(std::vector<std::shared_ptr<GameObject>> gameObjects)
{
    ZoneScoped;
    // CRITICAL: Check if ID buffer needs recreation due to window resize
    if (idBufferWidth != screenWidth || idBufferHeight != screenHeight)
    {
        std::cout << "ID buffer size mismatch! Buffer: " << idBufferWidth << "x" << idBufferHeight
                  << " Screen: " << screenWidth << "x" << screenHeight << std::endl;
        setupIDBuffer(); // Recreate with correct size
    }

    if (IDFrameBuffer == 0)
    {
        std::cerr << "ID Framebuffer not initialized!" << std::endl;
        return;
    }

    // SAVE OpenGL state that might interfere
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean cullFaceWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    // 1. Bind the ID framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, IDFrameBuffer);

    // 2. Set viewport to EXACT framebuffer size
    glViewport(0, 0, screenWidth, screenHeight);
    // std::cout << "ID buffer viewport set to: " << screenWidth << "x" << screenHeight << std::endl;

    // 3. DISABLE problematic states
    glDisable(GL_BLEND);     // CRITICAL: Prevents ID mixing
    glDisable(GL_CULL_FACE); // Ensures all faces are rendered consistently

    // 4. Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    // 5. Clear buffers
    unsigned int clearValue = 0;
    glClearBufferuiv(GL_COLOR, 0, &clearValue);
    glClear(GL_DEPTH_BUFFER_BIT);

    // 6. Get and use the ID shader
    Shader *idShader = getShader("id_shader");
    if (!idShader)
    {
        std::cerr << "ID Shader not found!" << std::endl;
        // Restore state before returning
        if (blendWasEnabled)
            glEnable(GL_BLEND);
        if (cullFaceWasEnabled)
            glEnable(GL_CULL_FACE);
        glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    idShader->use();

    // 7. Set camera matrices (MUST be same as main render)
    idShader->setMat4("view", viewMatrix);
    idShader->setMat4("projection", projectionMatrix);

    // 8. Render objects
    int renderedCount = 0;
    for (const auto &gameObject : gameObjects)
    {
        if (!gameObject)
            continue;

        // Validate object ID
        if (gameObject->ID == 0)
        {
            std::cerr << "Warning: GameObject has ID 0 (reserved for background)" << std::endl;
            continue;
        }
        glm::mat4 model = gameObject->GetTransform();
        idShader->setUint("objectID", gameObject->ID);
        idShader->setMat4("model", model);

        // Draw the object's mesh
        if (!isInViewDistance(gameObject->position, currentCamera->Position, 1000.0f))
        {
            continue;
        }

        // Then frustum check
        if (!isInFrustum(gameObject->position, 1.0f, projectionMatrix * viewMatrix))
        {
            continue;
        }
        gameObject->model->Draw(*idShader);
        renderedCount++;
    }

    // std::cout << "Rendered " << renderedCount << " objects to ID buffer" << std::endl;

    // 9. RESTORE OpenGL state
    if (blendWasEnabled)
        glEnable(GL_BLEND);
    if (cullFaceWasEnabled)
        glEnable(GL_CULL_FACE);
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

    // 10. Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Check for errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in renderSceneToIDBuffer: " << error << std::endl;
    }
}

unsigned int RenderManager::getObjectId(int mouseX, int mouseY)
{
    // Check if coordinates are valid for current buffer size
    if (mouseX < 0 || mouseX >= idBufferWidth || mouseY < 0 || mouseY >= idBufferHeight)
    {
        std::cerr << "Mouse coordinates (" << mouseX << ", " << mouseY
                  << ") out of bounds for ID buffer size " << idBufferWidth << "x" << idBufferHeight << std::endl;
        return 0;
    }

    if (IDFrameBuffer == 0)
    {
        std::cerr << "ID Framebuffer is not initialized!" << std::endl;
        return 0;
    }

    // Bind framebuffer for reading
    glBindFramebuffer(GL_READ_FRAMEBUFFER, IDFrameBuffer);

    // Convert coordinates: Window Y (top-left) to OpenGL Y (bottom-left)
    int glY = idBufferHeight - 1 - mouseY;

    unsigned int pixelValue = 0;
    glReadPixels(mouseX, glY, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &pixelValue);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // Debug output
    std::cout << "Mouse: (" << mouseX << ", " << mouseY << ") -> GL: ("
              << mouseX << ", " << glY << ") -> Object ID: " << pixelValue << std::endl;

    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in getObjectId: " << error << std::endl;
        return 0;
    }

    return pixelValue;
}

// Optional: Helper function to debug the entire buffer (call sparingly!)
void RenderManager::debugIDBuffer()
{
    if (IDFrameBuffer == 0)
    {
        std::cerr << "ID Framebuffer is not initialized!" << std::endl;
        return;
    }

    std::vector<unsigned int> pixels(screenWidth * screenHeight);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, IDFrameBuffer);
    glReadPixels(0, 0, screenWidth, screenHeight, GL_RED_INTEGER, GL_UNSIGNED_INT, pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    std::cout << "--- ID Buffer Contents (non-zero values only) ---" << std::endl;
    for (int y = 0; y < screenHeight; ++y)
    {
        for (int x = 0; x < screenWidth; ++x)
        {
            int index = (screenHeight - 1 - y) * screenWidth + x;
            unsigned int objectID = pixels[index];
            if (objectID != 0)
            {
                std::cout << "Pixel (" << x << ", " << y << "): ID " << objectID << std::endl;
            }
        }
    }
    std::cout << "------------------------------------------------" << std::endl;
}

// unsigned int RenderManager::getObjectId(int mouseX, int mouseY)
// {
//     // Make sure the ID framebuffer is set up
//     if (IDFrameBuffer == 0)
//     {
//         std::cerr << "ID Framebuffer is not initialized!" << std::endl;
//         return 0; // Return a default value for 'no object'
//     }
//     std::vector<unsigned int> pixels(screenWidth * screenHeight);

//     // 2. Bind the ID framebuffer for reading.
//     // We specify GL_READ_FRAMEBUFFER to tell OpenGL we are only reading from it.
//     glBindFramebuffer(GL_READ_FRAMEBUFFER, IDFrameBuffer);

//     // 3. Read the entire pixel data from the framebuffer into the CPU vector.
//     // This is the most efficient way to get the data to the CPU in a single go.
//     //
//     // Parameters:
//     // x, y, width, height: The region of the framebuffer to read. We read the entire screen (0, 0, screenWidth, screenHeight).
//     // format: The format of the pixel data to be read. GL_RED_INTEGER corresponds to a single-channel integer format.
//     // type: The data type of the pixel values. GL_UNSIGNED_INT matches the GL_R32UI format.
//     // data: A pointer to the CPU-side memory where the data will be stored.
//     glReadPixels(0, 0, screenWidth, screenHeight, GL_RED_INTEGER, GL_UNSIGNED_INT, pixels.data());

//     // 4. Unbind the framebuffer to return to the default state.
//     glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

//     // 5. Loop through the CPU vector and print the values.
//     // This loop is very fast since the data is already in main memory.
//     std::cout << "--- ID Buffer Contents ---" << std::endl;
//     for (int y = 0; y < screenHeight; ++y)
//     {
//         for (int x = 0; x < screenWidth; ++x)
//         {
//             // Calculate the 1D index from the 2D coordinates.
//             // Remember that OpenGL's y-origin is at the bottom-left, so we need to flip the y-coordinate for intuitive printing.
//             int index = (screenHeight - 1 - y) * screenWidth + x;
//             unsigned int objectID = pixels[index];
//             if (objectID != 0)
//                 std::cout << objectID << "\t";
//         }
//         // std::cout << std::endl;
//     }
//     std::cout << "--------------------------" << std::endl;

//     return 0;
// }

void RenderManager::useShader(GameObject &gameObject, Shader *shader, std::vector<glm::vec3> &lights, glm::mat4 lightSpaceMatrix)
{
    glm::mat4 model = gameObject.GetTransform();
    shader->use();
    shader->setMat4("projection", projectionMatrix);
    shader->setMat4("view", viewMatrix);
    shader->setFloat("time", glfwGetTime());
    shader->setVec3("viewPos", currentCamera->Position);
    shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
    shader->setMat4("model", model);
    shader->setVec3("objectColor", gameObject.color);

    shader->setVec3("lightColor", glm::vec3(1.0f, 0.0f, 0.0f));
    shader->setInt("shadowMap", 0);
    shader->setInt("debugMode", boneDebugMode);
    shader->setVec4("clipPlane", activeClipPlane);

    int lightCount = static_cast<int>(lights.size());
    shader->setInt("numLights", lightCount);

    // 2. Loop through and set each light's properties (stack buffers, no heap alloc)
    char nameBuf[32];
    for (int i = 0; i < lightCount; ++i)
    {
        snprintf(nameBuf, sizeof(nameBuf), "lights[%d].Position", i);
        shader->setVec3(nameBuf, lights[i]);
        snprintf(nameBuf, sizeof(nameBuf), "lights[%d].Color", i);
        shader->setVec3(nameBuf, glm::vec3(1.0f, 1.0f, 1.0f));
    }

    if (gameObject.shaderName == "terrain_tile_shader")
    {
        shader->setInt("terrainType", gameObject.terrainType);
        shader->setFloat("texTiling", 0.25f); // texture spans ~4 tiles

        // PBR terrain textures (2K): 3 material sets (diffuse + normal each)
        // Unit 0 = shadowMap (bound elsewhere)
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, t_grassDiff);
        shader->setInt("tex_grass_diff", 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, t_grassNor);
        shader->setInt("tex_grass_nor", 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, t_stoneDiff);
        shader->setInt("tex_stone_diff", 3);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, t_stoneNor);
        shader->setInt("tex_stone_nor", 4);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, t_rock2Diff);
        shader->setInt("tex_rock2_diff", 5);

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, t_rock2Nor);
        shader->setInt("tex_rock2_nor", 6);

        // Optional PBR maps
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, t_grassAO);
        shader->setInt("tex_grass_ao", 7);
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, t_stoneAO);
        shader->setInt("tex_stone_ao", 8);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, t_grassRough);
        shader->setInt("tex_grass_rough", 9);
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, t_stoneRough);
        shader->setInt("tex_stone_rough", 10);
        shader->setBool("hasGrassAO", t_grassAO != 0);
        shader->setBool("hasStoneAO", t_stoneAO != 0);
        shader->setBool("hasGrassRough", t_grassRough != 0);
        shader->setBool("hasStoneRough", t_stoneRough != 0);
    }

    // ── PBR shaders ───────────────────────────────────────────────────────────
    if (gameObject.shaderName == "pbr_direct")
    {
        // Uniform material: derive albedo from the object's colour field,
        // use sensible physical defaults for metallic/roughness/ao.
        // These can later be exposed per-object or via ImGui.
        shader->setVec3("albedo", gameObject.color);
        shader->setFloat("metallic", 0.0f);  // stone/wood: dielectric
        shader->setFloat("roughness", 0.5f); // medium roughness
        shader->setFloat("ao", 1.0f);        // no occlusion by default

        // Shadow map — slot 0 (already bound by useShader header)
        shader->setInt("shadowMap", 0);

        // PBR lights need higher radiance values than toon lights
        // because we divide by distance² — scale colour from 1 → 150
        for (int i = 0; i < static_cast<int>(lights.size()); ++i)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "lights[%d].Color", i);
            shader->setVec3(buf, glm::vec3(150.0f));
        }
    }

    if (gameObject.shaderName == "pbr_textured")
    {
        // Tell each sampler which texture unit it lives on.
        // Unit 0 = shadowMap (bound in the common header above).
        shader->setInt("shadowMap", 0);
        shader->setInt("albedoMap", 1);
        shader->setInt("normalMap", 2);
        shader->setInt("metallicMap", 3);
        shader->setInt("roughnessMap", 4);
        shader->setInt("aoMap", 5);
        shader->setInt("depthMap", 7);
        shader->setFloat("texTiling", 1.0f);

        // Bind PBR textures from the named material library.
        auto it = pbrMaterials.find(gameObject.materialName);
        if (it != pbrMaterials.end())
        {
            const PBRMaterial &mat = it->second;
            auto bindTex = [](unsigned int id, int unit)
            {
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(GL_TEXTURE_2D, id);
            };
            bindTex(mat.albedo, 1);
            bindTex(mat.normal, 2);
            bindTex(mat.metallic, 3);
            bindTex(mat.roughness, 4);
            bindTex(mat.ao, 5);
            bindTex(mat.displacement, 7);
            // Enable POM only when a displacement map is actually provided.
            shader->setFloat("heightScale", mat.displacement ? 0.05f : 0.0f);
        }
        else
        {
            shader->setFloat("heightScale", 0.0f);
        }

        // Skybox cubemap for env reflections — slot 6 (above the 5 material maps)
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        shader->setInt("envMap", 6);

        // Scale lights for PBR inverse-square attenuation
        for (int i = 0; i < static_cast<int>(lights.size()); ++i)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "lights[%d].Color", i);
            shader->setVec3(buf, glm::vec3(150.0f));
        }
    }

    if (gameObject.shaderName == "pbr_model_textured")
    {
        // texture_diffuse1 (slot 0) and texture_normal1 (slot 1) are bound
        // automatically by Mesh::Draw — we don't touch those slots here.
        //
        // Shadow map goes to slot 15: guaranteed not to be overwritten by
        // Mesh::Draw which iterates from slot 0 upward (models rarely exceed 8).
        glActiveTexture(GL_TEXTURE15);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        shader->setInt("shadowMap", 15);

        // Skybox cubemap for env reflections — slot 14 (safe from Mesh::Draw)
        glActiveTexture(GL_TEXTURE14);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        shader->setInt("envMap", 14);

        // Scale lights for PBR inverse-square attenuation (same as other PBR shaders)
        for (int i = 0; i < static_cast<int>(lights.size()); ++i)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "lights[%d].Color", i);
            shader->setVec3(buf, glm::vec3(150.0f));
        }
    }

    if (gameObject.shaderName == "water_noG")
    {
        // Camera and view uniforms
        glUniform3fv(glGetUniformLocation(shader->ID, "cameraPos"), 1, &currentCamera->Position[0]);
        glUniform1f(glGetUniformLocation(shader->ID, "nearPlane"), 0.1f);
        glUniform1f(glGetUniformLocation(shader->ID, "farPlane"), 1000.0f);

        // Set these in your C++ code:
        glUniform1i(glGetUniformLocation(shader->ID, "depthBands"), 4);
        glUniform1i(glGetUniformLocation(shader->ID, "foamBands"), 3);
        glUniform1f(glGetUniformLocation(shader->ID, "cellShadingStrength"), 0.4f);
        glUniform1f(glGetUniformLocation(shader->ID, "foamStrength"), 1.0f);

        // Lighting uniforms
        // glUniform3fv(glGetUniformLocation(shader->ID, "lightDir"), 1, &lightDirection[0]);
        // glUniform3fv(glGetUniformLocation(shader->ID, "lightColor"), 1, &lightColor[0]);
        // glUniformMatrix4fv(glGetUniformLocation(shader->ID, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);

        // Water color properties (based on your reference image)
        glm::vec3 shallowColor = glm::vec3(0.4f, 0.7f, 0.9f); // Light turquoise
        glm::vec3 deepColor = glm::vec3(0.1f, 0.4f, 0.6f);    // Deeper blue
        glUniform3fv(glGetUniformLocation(shader->ID, "waterColorShallow"), 1, &shallowColor[0]);
        glUniform3fv(glGetUniformLocation(shader->ID, "waterColorDeep"), 1, &deepColor[0]);

        // Water behavior parameters
        glUniform1f(glGetUniformLocation(shader->ID, "waterTransparency"), 0.25f);
        glUniform1f(glGetUniformLocation(shader->ID, "foamThreshold"), 1.5f);
        // glUniform1f(glGetUniformLocation(shader->ID, "foamStrength"), 3.2f);
        glUniform1f(glGetUniformLocation(shader->ID, "time"), glfwGetTime());

        // Bind textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glUniform1i(glGetUniformLocation(shader->ID, "depthTexture"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures.at("foam").get()->id);
        glUniform1i(glGetUniformLocation(shader->ID, "foamTexture"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textures.at("water_normal").get()->id);
        glUniform1i(glGetUniformLocation(shader->ID, "normalMap"), 2);

        // glActiveTexture(GL_TEXTURE3);
        // glBindTexture(GL_TEXTURE_2D, shadowMap);
        // glUniform1i(glGetUniformLocation(shader->ID, "shadowMap"), 3);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
}

// Overloaded version with vec4 color (includes alpha)
void RenderManager::renderGameObjectWithColor(GameObject &gameObject, Shader shader, glm::vec4 color)
{
    ZoneScoped;
    if (!isInViewDistance(gameObject.position, currentCamera->Position, 1000.0f))
    {
        return;
    }

    // Then frustum check
    if (!isInFrustum(gameObject.position, 1.0f, projectionMatrix * viewMatrix))
    {
        return;
    }
    shader.use();

    shader.setVec4("objectColor", color);

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 scaling = glm::scale(glm::mat4(1.0f), gameObject.scale);
    model = glm::translate(model, gameObject.position) * scaling;
    shader.setMat4("projection", projectionMatrix);
    shader.setMat4("view", viewMatrix);
    shader.setMat4("model", model);
    shader.setFloat("time", glfwGetTime());

    // Optional: Set lighting uniforms if using lighting
    shader.setVec3("lightPos", glm::vec3(10.0f, 10.0f, 10.0f));
    shader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
    shader.setVec3("viewPos", glm::vec3(0.0f, 0.0f, 3.0f));

    // Enable blending if using alpha
    if (color.a < 1.0f)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Draw the model
    gameObject.model->Draw(shader);

    // Disable blending after drawing
    if (color.a < 1.0f)
    {
        glDisable(GL_BLEND);
    }
}

void RenderManager::renderGhostObject(GameObject &gameObject, glm::vec3 position, float alpha)
{
    ZoneScoped;
    if (!isInViewDistance(position, currentCamera->Position, 1000.0f))
    {
        return;
    }

    if (!isInFrustum(position, 1.0f, projectionMatrix * viewMatrix))
    {
        return;
    }

    Shader *shader = getShader("simple_color_shader");
    if (!shader)
        return;

    shader->use();

    // Flickering effect using time - vary the brightness
    float time = static_cast<float>(glfwGetTime());
    float flicker = 0.5f + 0.5f * sin(time * 10.0f); // Oscillates between 0 and 1

    // Subdued color (desaturated), with flickering brightness
    glm::vec3 baseColor = gameObject.color;
    glm::vec3 desaturated = glm::mix(baseColor, glm::vec3(0.5f), 0.5f); // 50% desaturation
    float flickerBrightness = 0.3f + 0.4f * flicker;                    // Flickers between 30% and 70% brightness
    glm::vec3 ghostColor = desaturated * flickerBrightness;

    shader->setVec3("objectColor", ghostColor);

    // Use destination X and Z, but keep the object's current Y position
    glm::vec3 ghostPosition = glm::vec3(position.x, gameObject.position.y, position.z);

    // Build T × R × S using the ghost position but the object's rotation and scale
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), ghostPosition);
    glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), gameObject.rotation.x, glm::vec3(1, 0, 0));
    glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), gameObject.rotation.y, glm::vec3(0, 1, 0));
    glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), gameObject.rotation.z, glm::vec3(0, 0, 1));
    glm::mat4 rotationMatrix = rotationZ * rotationY * rotationX;
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), gameObject.scale);
    glm::mat4 model = translationMatrix * rotationMatrix * scaleMatrix;
    shader->setMat4("projection", projectionMatrix);
    shader->setMat4("view", viewMatrix);
    shader->setMat4("model", model);

    // Use polygon offset to prevent z-fighting with the actual object
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);

    gameObject.model->Draw(*shader);

    glDisable(GL_POLYGON_OFFSET_FILL);
}

void RenderManager::renderGameObjectWithTexture(GameObject &gameObject, Shader shader, unsigned int textureID)
{
    ZoneScoped;
    if (!isInViewDistance(gameObject.position, currentCamera->Position, 1000.0f))
    {
        return;
    }

    // Then frustum check
    if (!isInFrustum(gameObject.position, 1.0f, projectionMatrix * viewMatrix))
    {
        return;
    }
    shader.use();
    // Check if texture actually bound
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 scaling = glm::scale(glm::mat4(1.0f), gameObject.scale);
    model = glm::translate(model, gameObject.position) * scaling;
    shader.setMat4("projection", projectionMatrix);
    shader.setMat4("view", viewMatrix);
    shader.setMat4("model", model);
    shader.setFloat("time", glfwGetTime());
    gameObject.model->SetDiffuseTexture(textureID);
    gameObject.model->Draw(shader);
}

void RenderManager::renderCameraAttachedObject(GameObject &gameObject, Shader shader)
{
    ZoneScoped;
    shader.use();

    glm::mat4 model = glm::mat4(1.0f);

    // Apply object's local offset (from JSON position data)
    model = glm::translate(model, gameObject.position);

    // Apply camera's inverse rotation to keep object oriented with camera
    glm::mat3 cameraRotationInverse = glm::transpose(glm::mat3(currentCamera->GetViewMatrix()));
    model = glm::mat4(cameraRotationInverse) * model;

    // Translate to camera position
    model = glm::translate(glm::mat4(1.0f), currentCamera->Position) * model;

    // Apply scaling
    model = glm::scale(model, gameObject.scale);

    shader.setMat4("projection", projectionMatrix);
    shader.setMat4("view", viewMatrix);
    shader.setMat4("model", model);
    shader.setFloat("time", glfwGetTime());

    gameObject.model->Draw(shader);
}

// Enhanced version with customizable axes, colors, and center highlighting
void RenderManager::renderGridAdvanced(glm::mat4 view, int gridSize = 20, float spacing = 1.0f,
                                       float lineThickness = 0.02f, bool drawCenterLines = true,
                                       bool drawYAxis = false, float yAxisHeight = 10.0f)
{
    ZoneScoped;
    float halfGrid = (gridSize * spacing) * 0.5f;

    // Render main grid lines
    for (int i = 0; i <= gridSize; i++)
    {
        float offset = -halfGrid + (i * spacing);
        bool isCenterLine = (i == gridSize / 2);

        // Adjust thickness for center lines
        float currentThickness = isCenterLine && drawCenterLines ? lineThickness * 2.0f : lineThickness;

        // X-axis parallel lines (varying along Z)
        glm::vec3 xStartPoint(-halfGrid, 0.0f, offset);
        glm::vec3 xDirection(1.0f, 0.0f, 0.0f);
        renderLine(xStartPoint, xDirection, view, currentThickness, gridSize * spacing);

        // Z-axis parallel lines (varying along X)
        glm::vec3 zStartPoint(offset, 0.0f, -halfGrid);
        glm::vec3 zDirection(0.0f, 0.0f, 1.0f);
        renderLine(zStartPoint, zDirection, view, currentThickness, gridSize * spacing);
    }

    // Optional Y-axis (vertical line at center)
    if (drawYAxis)
    {
        glm::vec3 yStartPoint(0.0f, 0.0f, 0.0f);
        glm::vec3 yDirection(0.0f, 1.0f, 0.0f);
        renderLine(yStartPoint, yDirection, view, lineThickness * 1.5f, yAxisHeight);
    }
}

// Infinite grid version that follows the camera
void RenderManager::renderInfiniteGrid(glm::mat4 view, glm::vec3 cameraPosition, Shader shader, float spacing = 1.0f, float fadeDistance = 50.0f,
                                       float lineThickness = 0.02f, int visibleRange = 100)
{
    ZoneScoped;
    gridShader = shader.ID;
    initializeGridBuffers();

    // Prepare instance data
    std::vector<GridLineInstance> instances;
    instances.reserve(visibleRange * 2); // Pre-allocate for performance

    // Snap camera to grid
    int centerX = static_cast<int>(std::round(cameraPosition.x / spacing));
    int centerZ = static_cast<int>(std::round(cameraPosition.z / spacing));
    float gridCenterX = centerX * spacing;
    float gridCenterZ = centerZ * spacing;
    int halfRange = visibleRange / 2;

    // X-parallel lines
    for (int i = -halfRange; i <= halfRange; i++)
    {
        float z = gridCenterZ + (i * spacing);
        float distanceFromCamera = std::abs(z - cameraPosition.z);
        float fadeFactor = std::max(0.1f, 1.0f - (distanceFromCamera / fadeDistance));
        float adjustedThickness = lineThickness * fadeFactor;

        if (adjustedThickness > 0.01f)
        {
            GridLineInstance instance;
            instance.startPos = glm::vec3(gridCenterX - halfRange * spacing, 0.0f, z);
            instance.direction = glm::vec3(1.0f, 0.0f, 0.0f);
            instance.thickness = adjustedThickness;
            instance.length = visibleRange * spacing;

            // Check if this is Z-axis
            bool isZAxis = (std::abs(z) < spacing * 0.1f);
            if (isZAxis)
            {
                instance.color = glm::vec3(0.3f, 0.3f, 0.8f); // Blue
                instance.thickness *= 2.0f;
            }
            else
            {
                instance.color = glm::vec3(0.3f, 0.3f, 0.3f); // Gray
            }

            instances.push_back(instance);
        }
    }

    // Z-parallel lines
    for (int i = -halfRange; i <= halfRange; i++)
    {
        float x = gridCenterX + (i * spacing);
        float distanceFromCamera = std::abs(x - cameraPosition.x);
        float fadeFactor = std::max(0.1f, 1.0f - (distanceFromCamera / fadeDistance));
        float adjustedThickness = lineThickness * fadeFactor;

        if (adjustedThickness > 0.01f)
        {
            GridLineInstance instance;
            instance.startPos = glm::vec3(x, 0.0f, gridCenterZ - halfRange * spacing);
            instance.direction = glm::vec3(0.0f, 0.0f, 1.0f);
            instance.thickness = adjustedThickness;
            instance.length = visibleRange * spacing;

            // Check if this is X-axis
            bool isXAxis = (std::abs(x) < spacing * 0.1f);
            if (isXAxis)
            {
                instance.color = glm::vec3(0.8f, 0.3f, 0.3f); // Red
                instance.thickness *= 2.0f;
            }
            else
            {
                instance.color = glm::vec3(0.3f, 0.3f, 0.3f); // Gray
            }

            instances.push_back(instance);
        }
    }

    if (instances.empty())
        return;

    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, gridInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 instances.size() * sizeof(GridLineInstance),
                 instances.data(), GL_DYNAMIC_DRAW);

    // Render all lines in one draw call
    glUseProgram(gridShader);
    glUniformMatrix4fv(glGetUniformLocation(gridShader, "view"), 1, GL_FALSE, &view[0][0]);

    glBindVertexArray(gridVAO);
    glDisable(GL_CULL_FACE);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, instances.size()); // 36 vertices for cube
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}

void RenderManager::renderGrid(glm::mat4 view, glm::mat4 projection)
{
    static unsigned int gridVAO = 0;
    static unsigned int gridVBO = 0;
    static unsigned int gridEBO = 0;
    static bool initialized = false;

    // Initialize grid mesh once
    if (!initialized)
    {
        float size = 1000.0f;
        float vertices[] = {
            -size, 0.0f, -size,
            size, 0.0f, -size,
            size, 0.0f, size,
            -size, 0.0f, size};

        unsigned int indices[] = {0, 1, 2, 2, 3, 0};

        glGenVertexArrays(1, &gridVAO);
        glGenBuffers(1, &gridVBO);
        glGenBuffers(1, &gridEBO);

        glBindVertexArray(gridVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
        initialized = true;
    }

    // Set uniforms (assumes grid shader is already active)
    glm::mat4 model = glm::mat4(1.0f);

    GLint currentShader;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentShader);

    int u_model = glGetUniformLocation(currentShader, "model");
    int u_view = glGetUniformLocation(currentShader, "view");
    int u_projection = glGetUniformLocation(currentShader, "projection");

    if (u_model != -1)
        glUniformMatrix4fv(u_model, 1, GL_FALSE, glm::value_ptr(model));
    if (u_view != -1)
        glUniformMatrix4fv(u_view, 1, GL_FALSE, glm::value_ptr(view));
    if (u_projection != -1)
        glUniformMatrix4fv(u_projection, 1, GL_FALSE, glm::value_ptr(projection));

    // Render state for transparent grid
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    // glDisable(GL_DEPTH_TEST); // Disable depth testing completely
    // glDepthMask(GL_FALSE);

    // Draw grid
    glBindVertexArray(gridVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Restore render state
    // glDepthMask(GL_TRUE);
    // glEnable(GL_DEPTH_TEST); // Re-enable depth testing
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

void RenderManager::initializeGridBuffers()
{
    if (gridInitialized)
        return;

    // Create base line geometry (unit cube that we'll transform)
    float baseLineVertices[] = {
        // Simple box vertices (will be transformed by instances)
        -0.5f, -0.5f, 0.0f, // Bottom face
        0.5f, -0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,

        -0.5f, -0.5f, 1.0f, // Top face
        0.5f, -0.5f, 1.0f,
        0.5f, 0.5f, 1.0f,
        0.5f, 0.5f, 1.0f,
        -0.5f, 0.5f, 1.0f,
        -0.5f, -0.5f, 1.0f,

        // Add side faces...
        -0.5f, -0.5f, 0.0f, // Front
        -0.5f, 0.5f, 0.0f,
        -0.5f, 0.5f, 1.0f,
        -0.5f, 0.5f, 1.0f,
        -0.5f, -0.5f, 1.0f,
        -0.5f, -0.5f, 0.0f,

        0.5f, -0.5f, 0.0f, // Back
        0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 1.0f,
        0.5f, 0.5f, 1.0f,
        0.5f, -0.5f, 1.0f,
        0.5f, -0.5f, 0.0f,

        -0.5f, -0.5f, 0.0f, // Left
        0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 1.0f,
        0.5f, -0.5f, 1.0f,
        -0.5f, -0.5f, 1.0f,
        -0.5f, -0.5f, 0.0f,

        -0.5f, 0.5f, 0.0f, // Right
        0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 1.0f,
        0.5f, 0.5f, 1.0f,
        -0.5f, 0.5f, 1.0f,
        -0.5f, 0.5f, 0.0f};

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glGenBuffers(1, &gridInstanceVBO);

    glBindVertexArray(gridVAO);

    // Base geometry
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(baseLineVertices), baseLineVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    // Instance data buffer (will be updated each frame)
    glBindBuffer(GL_ARRAY_BUFFER, gridInstanceVBO);

    // Instance attributes
    // startPos (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GridLineInstance),
                          (void *)offsetof(GridLineInstance, startPos));
    glVertexAttribDivisor(1, 1);

    // direction (vec3)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GridLineInstance),
                          (void *)offsetof(GridLineInstance, direction));
    glVertexAttribDivisor(2, 1);

    // thickness (float)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GridLineInstance),
                          (void *)offsetof(GridLineInstance, thickness));
    glVertexAttribDivisor(3, 1);

    // length (float)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(GridLineInstance),
                          (void *)offsetof(GridLineInstance, length));
    glVertexAttribDivisor(4, 1);

    // color (vec3)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(GridLineInstance),
                          (void *)offsetof(GridLineInstance, color));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
    gridInitialized = true;
}

void RenderManager::renderGrassPoints(const std::vector<glm::vec3> &positions)
{
    ZoneScoped;
    static unsigned int grassPointsVAO = 0;
    static unsigned int grassPointsVBO = 0;

    if (grassPointsVAO == 0)
    {
        glGenVertexArrays(1, &grassPointsVAO);
        glGenBuffers(1, &grassPointsVBO);

        glBindVertexArray(grassPointsVAO);
        glBindBuffer(GL_ARRAY_BUFFER, grassPointsVBO);

        // Position attribute only
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    if (!positions.empty())
    {
        glBindBuffer(GL_ARRAY_BUFFER, grassPointsVBO);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), &positions[0], GL_DYNAMIC_DRAW);

        glBindVertexArray(grassPointsVAO);
        glDrawArrays(GL_POINTS, 0, positions.size());
        glBindVertexArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void RenderManager::initializeShaders()
{
    // Two-file shaders (vertex + fragment)
    shaders["depth_pre_pass"] = std::make_shared<Shader>("shaders/depth_pre_pass.vs", "shaders/depth_pre_pass.fs");
    shaders["water_shader"] = std::make_shared<Shader>("shaders/water.vs", "shaders/water.fs");
    shaders["terrain_shader"] = std::make_shared<Shader>("shaders/g_buffer_2.vs", "shaders/g_buffer.fs");
    shaders["geometry_pass_shader"] = std::make_shared<Shader>("shaders/g_buffer.vs", "shaders/g_buffer.fs");
    shaders["selected_shader"] = std::make_shared<Shader>("shaders/g_buffer.vs", "shaders/g_buffer_selected.fs");
    shaders["lighting_pass_shader"] = std::make_shared<Shader>("shaders/deferred_shading.vs", "shaders/deferred_shading.fs");
    shaders["light_box_shader"] = std::make_shared<Shader>("shaders/deferred_light_box.vs", "shaders/deferred_light_box.fs");
    shaders["skybox_shader"] = std::make_shared<Shader>("shaders/cubemap.vs", "shaders/cubemap.fs");
    shaders["simple_shader"] = std::make_shared<Shader>("shaders/shader.vs", "shaders/shader.fs");
    shaders["simple_color_shader"] = std::make_shared<Shader>("shaders/shader.vs", "shaders/shader_flat_color.fs");
    shaders["textured_shader"] = std::make_shared<Shader>("shaders/shader.vs", "shaders/shader_textured.fs");
    shaders["rock_shader"] = std::make_shared<Shader>("shaders/shader.vs", "shaders/rock.fs");
    shaders["debug_shader"] = std::make_shared<Shader>("shaders/debug.vs", "shaders/debug.fs");
    shaders["model_shader"] = std::make_shared<Shader>("shaders/model.vs", "shaders/model.fs");
    shaders["smoke_shader"] = std::make_shared<Shader>("shaders/smoke.vs", "shaders/smoke.fs");
    shaders["grid_shader"] = std::make_shared<Shader>("shaders/grid.vs", "shaders/grid.fs");
    shaders["grid_shader_2"] = std::make_shared<Shader>("shaders/grid_2.vs", "shaders/grid_2.fs");
    shaders["water_noG"] = std::make_shared<Shader>("shaders/water_2.vs", "shaders/water_2.fs");
    // No geometry shader — water.vs already computes smooth analytic normals via
    // FBM gradient finite-differences; flat per-face GS normals cause visible
    // faceting on large quads.
    shaders["water_forward"] = std::make_shared<Shader>("shaders/water.vs",
                                                        "shaders/water_forward.fs");
    shaders["id_shader"] = std::make_shared<Shader>("shaders/id_shader.vs", "shaders/id_shader.fs");
    shaders["line_shader"] = std::make_shared<Shader>("shaders/line_shader.vs", "shaders/line_shader.fs", "shaders/line_shader.gs");
    shaders["tile_shader"] = std::make_shared<Shader>("shaders/tile_shader.vs", "shaders/tile_shader.fs");
    shaders["dune_shader"] = std::make_shared<Shader>("shaders/sand_terrain.vs", "shaders/sand_terrain.fs");
    shaders["terrain_tile_shader"] = std::make_shared<Shader>("shaders/terrain_tile.vs", "shaders/terrain_tile.fs");
    shaders["ground_shader"] = std::make_shared<Shader>("shaders/ground.vs", "shaders/ground.fs");
    shaders["proc_terrain"] = std::make_shared<Shader>("shaders/proc_terrain.vs",
                                                       "shaders/proc_terrain.fs",
                                                       "shaders/proc_terrain.gs");

    // ── PBR shader family ─────────────────────────────────────────────────────
    // pbr_direct   — Cook-Torrance BRDF, material params as uniforms
    // pbr_textured — same BRDF, material params from texture maps
    // Both share the same vertex shader (pbr.vs).
    shaders["pbr_direct"] = std::make_shared<Shader>("shaders/pbr.vs", "shaders/pbr_direct.fs");
    shaders["pbr_textured"] = std::make_shared<Shader>("shaders/pbr.vs", "shaders/pbr_textured.fs");
    // Reads embedded model textures (texture_diffuse1 / texture_normal1).
    // Uses shadow map at slot 15 to avoid collision with Mesh::Draw bindings.
    shaders["pbr_model_textured"] = std::make_shared<Shader>("shaders/pbr.vs", "shaders/pbr_model_textured.fs");

    // Decode terrain textures in parallel (stbi_load is thread-safe); upload on main thread.
    struct RawPixels
    {
        unsigned char *data = nullptr;
        int w = 0, h = 0, nc = 0;
    };
    const std::vector<std::string> terrainFiles = {
        "assets/bark_08_4k/bark_08_baseColor_4k.png",                       // 0 → grassDiff, rock2Diff
        "assets/bark_08_4k/bark_08_normal_gl_4k.png",                       // 1 → grassNor,  rock2Nor
        "assets/bark_08_4k/bark_08_ambientOcclusion_4k.png",                // 2 → grassAO
        "assets/bark_08_4k/bark_08_roughness_4k.png",                       // 3 → grassRough
        "assets/floor_tiles_16_4k/floor_tiles_16__basecolor_4k.png",        // 4 → stoneDiff
        "assets/floor_tiles_16_4k/floor_tiles_16__normal_gl_4k.png",        // 5 → stoneNor
        "assets/floor_tiles_16_4k/floor_tiles_16__ambientocclusion_4k.png", // 6 → stoneAO
        "assets/floor_tiles_16_4k/floor_tiles_16__roughness_4k.png",        // 7 → stoneRough
    };

    // Phase 1: decode in parallel
    std::vector<std::future<RawPixels>> decFutures;
    for (const auto &path : terrainFiles)
    {
        decFutures.push_back(std::async(std::launch::async, [path]()
                                        {
            RawPixels r;
            r.data = stbi_load(path.c_str(), &r.w, &r.h, &r.nc, 0);
            return r; }));
    }

    // Phase 2: GPU upload on main thread (GL calls must stay here)
    auto uploadPixels = [](RawPixels r, const std::string &path) -> unsigned int
    {
        if (!r.data)
        {
            std::cout << "Terrain tex FAILED: " << path << std::endl;
            return 0;
        }
        unsigned int id;
        glGenTextures(1, &id);
        GLenum fmt = r.nc == 1 ? GL_RED : r.nc == 3 ? GL_RGB
                                                    : GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, r.w, r.h, 0, fmt, GL_UNSIGNED_BYTE, r.data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(r.data);
        return id;
    };

    std::vector<unsigned int> tids(terrainFiles.size());
    for (size_t i = 0; i < terrainFiles.size(); ++i)
        tids[i] = uploadPixels(decFutures[i].get(), terrainFiles[i]);

    t_grassDiff = tids[0];
    t_grassNor = tids[1];
    t_grassAO = tids[2];
    t_grassRough = tids[3];
    t_stoneDiff = tids[4];
    t_stoneNor = tids[5];
    t_stoneAO = tids[6];
    t_stoneRough = tids[7];
    t_rock2Diff = tids[0];
    t_rock2Nor = tids[1]; // rock2 reuses grass textures

    terrainPaths[0][0] = terrainFiles[0];
    terrainPaths[0][1] = terrainFiles[1];
    terrainPaths[0][2] = terrainFiles[2];
    terrainPaths[0][3] = terrainFiles[3];
    terrainPaths[1][0] = terrainFiles[4];
    terrainPaths[1][1] = terrainFiles[5];
    terrainPaths[1][2] = terrainFiles[6];
    terrainPaths[1][3] = terrainFiles[7];
    terrainPaths[2][0] = terrainFiles[0];
    terrainPaths[2][1] = terrainFiles[1];
    terrainPaths[2][2] = terrainFiles[2];
    terrainPaths[2][3] = terrainFiles[3];

    // Three-file shaders (vertex + fragment + geometry)
    shaders["simple_depth_shader"] = std::make_shared<Shader>("shaders/simple_depth_shader.vs",
                                                              "shaders/simple_depth_shader.fs",
                                                              "shaders/simple_depth_shader.gs");
    shaders["grass_shader"] = std::make_shared<Shader>("shaders/grass.vs",
                                                       "shaders/grass.fs");
    //    "shaders/grass.gs");

    shaders["rain_shader"] = std::make_shared<Shader>("shaders/rain.vs", "shaders/rain.fs");

    initRainSystem();
}

// ── PBR material loader ───────────────────────────────────────────────────────
// Iterates the serialiser's named material library and uploads every texture
// map to the GPU.  Maps that are empty strings are skipped (ID stays 0).
// Already-loaded paths are served from the texture cache, so re-calling this
// after hot-swap is cheap.
void RenderManager::loadPBRMaterials(const std::unordered_map<std::string, PBRMaterialDef> &defs)
{
    for (const auto &[name, def] : defs)
    {
        PBRMaterial mat;
        auto loadIfPresent = [&](const std::string &path) -> unsigned int
        {
            if (path.empty())
                return 0;
            return loadAndCacheTexture(path, path);
        };
        mat.albedo = loadIfPresent(def.albedo);
        mat.normal = loadIfPresent(def.normal);
        mat.metallic = loadIfPresent(def.metallic);
        mat.roughness = loadIfPresent(def.roughness);
        mat.ao = loadIfPresent(def.ao);
        mat.displacement = loadIfPresent(def.displacement);
        pbrMaterials[name] = mat;
        std::cout << "PBR material loaded: " << name << std::endl;
    }
}

void RenderManager::initializeDepthFBO()
{
    // Create depth framebuffer
    glGenFramebuffers(1, &depthFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);

    // Create depth texture
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    // Must match the viewport used in renderShadowPass — previously this was
    // screenWidth×screenHeight, meaning only the top-left 1024×1024 of a
    // 1920×1080 texture had valid data. textureSize() in the FS returned the
    // full screen size so PCF offsets were wrong, and the rest of the map had
    // garbage depths causing large portions of the scene to be incorrectly lit.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_WIDTH, SHADOW_HEIGHT,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // CLAMP_TO_BORDER with white: fragments outside the shadow frustum sample
    // depth=1 (max), so they always pass the shadow test and appear fully lit.
    // CLAMP_TO_EDGE was wrong — it repeated the edge shadow value outside the
    // frustum, causing false shadows along the borders.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Attach depth texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    // Disable color buffer
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Error: Depth framebuffer not complete!" << std::endl;
        // Cleanup on failure
        glDeleteFramebuffers(1, &depthFBO);
        glDeleteTextures(1, &depthTexture);
        depthFBO = depthTexture = 0;
    }

    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

unsigned int RenderManager::loadAndCacheTexture(const std::string &name, const std::string &path)
{
    auto it = textureCache.find(name);
    if (it != textureCache.end())
    {
        return it->second;
    }

    unsigned int texture = loadTexture(name.c_str(), path.c_str());
    textureCache[name] = texture;
    return texture;
}

bool RenderManager::reloadTerrainSlot(int slot, int mapType, const std::string &path)
{
    unsigned int newTex = loadTexture(path, path.c_str());
    if (newTex == 0)
        return false;

    // Map slot/mapType to the right member variable
    unsigned int *targets[3][4] = {
        {&t_grassDiff, &t_grassNor, &t_grassAO, &t_grassRough},
        {&t_stoneDiff, &t_stoneNor, &t_stoneAO, &t_stoneRough},
        {&t_rock2Diff, &t_rock2Nor, &t_grassAO, &t_grassRough}, // rock2 AO/rough share grass
    };

    if (slot < 0 || slot > 2 || mapType < 0 || mapType > 3)
        return false;

    glDeleteTextures(1, targets[slot][mapType]);
    *targets[slot][mapType] = newTex;
    terrainPaths[slot][mapType] = path;
    return true;
}

std::string RenderManager::getTerrainSlotPath(int slot, int mapType) const
{
    if (slot < 0 || slot > 2 || mapType < 0 || mapType > 3)
        return "";
    return terrainPaths[slot][mapType];
}

void RenderManager::generateGrassInstances(const glm::vec3 &center, float radius, int density)
{
    if (grassInstancesGenerated)
        return;

    grassInstances.clear();
    grassInstances.reserve(density);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-radius, radius);
    std::uniform_real_distribution<float> rotDist(0.0f, 2.0f * M_PI);
    std::uniform_real_distribution<float> scaleDist(0.7f, 1.3f);
    std::uniform_real_distribution<float> tintDist(0.8f, 1.2f);

    for (int i = 0; i < density; ++i)
    {
        GrassInstance instance;

        // Generate position in circular distribution
        float angle = rotDist(gen);
        float distance = std::sqrt(posDist(gen) * posDist(gen)) * radius;

        instance.position = center + glm::vec3(
                                         distance * std::cos(angle),
                                         0.0f,
                                         distance * std::sin(angle));

        instance.rotation = rotDist(gen);
        instance.scale = scaleDist(gen);

        // Add some color variation
        instance.tint = glm::vec3(
            tintDist(gen) * 0.4f + 0.2f, // Green variation
            tintDist(gen) * 0.8f + 0.6f, // Main green
            tintDist(gen) * 0.3f + 0.1f  // Blue tint
        );

        grassInstances.push_back(instance);
    }

    grassInstancesGenerated = true;
}

void RenderManager::setupGrassInstancing()
{
    if (grassInstanced || grassInstances.empty() || !grassModel)
    {
        return;
    }

    // Generate instance buffer once
    glGenBuffers(1, &grassInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, grassInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 grassInstances.size() * sizeof(GrassInstance),
                 grassInstances.data(),
                 GL_STATIC_DRAW);

    // Check for buffer creation errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error creating instance buffer: " << error << std::endl;
        return;
    }

    // Setup instance attributes for EACH mesh in the model
    for (auto &mesh : grassModel->meshes)
    {
        // CRITICAL: Bind the mesh's VAO before setting up attributes
        glBindVertexArray(mesh.VAO);

        // Bind instance buffer for this mesh
        glBindBuffer(GL_ARRAY_BUFFER, grassInstanceVBO);

        // Setup instance attributes

        // Instance position (location 3)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(GrassInstance),
                              (void *)offsetof(GrassInstance, position));
        glVertexAttribDivisor(3, 1); // Update once per instance

        // Instance rotation (location 4)
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(GrassInstance),
                              (void *)offsetof(GrassInstance, rotation));
        glVertexAttribDivisor(4, 1);

        // Instance scale (location 5)
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(GrassInstance),
                              (void *)offsetof(GrassInstance, scale));
        glVertexAttribDivisor(5, 1);

        // Instance tint (location 6)
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(GrassInstance),
                              (void *)offsetof(GrassInstance, tint));
        glVertexAttribDivisor(6, 1);

        // Check for errors after setting up each mesh
        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cerr << "OpenGL error setting up instancing for mesh: " << error << std::endl;
        }
    }

    // Unbind VAO and buffer
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    std::cout << "Grass instancing setup complete for " << grassInstances.size() << " instances" << std::endl;
    grassInstanced = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tile instancing
// ─────────────────────────────────────────────────────────────────────────────

void RenderManager::clearTileBatches()
{
    for (auto &[path, batch] : tileBatches)
        if (batch.instanceVBO)
            glDeleteBuffers(1, &batch.instanceVBO);
    tileBatches.clear();
}

void RenderManager::setupTileBatch(const std::string &modelPath, Model &model, size_t maxInstances)
{
    auto &batch = tileBatches[modelPath];
    if (batch.setupDone)
        return;

    glGenBuffers(1, &batch.instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(TileInstance), nullptr, GL_DYNAMIC_DRAW);

    for (auto &mesh : model.meshes)
    {
        glBindVertexArray(mesh.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);

        // mat4 instanceModel → locations 7, 8, 9, 10  (one vec4 per column)
        for (int col = 0; col < 4; col++)
        {
            GLuint loc = 7 + col;
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(TileInstance),
                                  (void *)(offsetof(TileInstance, modelMatrix) + col * sizeof(glm::vec4)));
            glVertexAttribDivisor(loc, 1);
        }

        // float instanceTerrainType → location 11
        glEnableVertexAttribArray(11);
        glVertexAttribPointer(11, 1, GL_FLOAT, GL_FALSE, sizeof(TileInstance),
                              (void *)offsetof(TileInstance, terrainType));
        glVertexAttribDivisor(11, 1);

        glBindVertexArray(0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    batch.setupDone = true;
}

void RenderManager::renderTilesInstanced(
    const std::vector<std::shared_ptr<GameObject>> &gameObjects,
    const std::vector<glm::vec3> &lightPos,
    const glm::mat4 &lightSpaceMatrix)
{
    // Collect instances grouped by model path
    std::unordered_map<std::string, std::vector<TileInstance>> perModel;
    std::unordered_map<std::string, Model *> pathToModel;

    for (auto &obj : gameObjects)
    {
        if (obj->shaderName != "terrain_tile_shader")
            continue;

        TileInstance inst;
        inst.modelMatrix = obj->getModelMatrix();
        inst.terrainType = static_cast<float>(obj->terrainType);
        perModel[obj->modelPath].push_back(inst);
        if (!pathToModel.count(obj->modelPath))
            pathToModel[obj->modelPath] = obj->model.get();
    }

    if (perModel.empty())
        return;

    Shader *shader = getShader("terrain_tile_shader");
    if (!shader)
        return;

    // Set shared uniforms once for all tile batches
    shader->use();
    shader->setMat4("projection", projectionMatrix);
    shader->setMat4("view", viewMatrix);
    shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
    shader->setVec4("clipPlane", activeClipPlane);
    shader->setFloat("texTiling", 1.0f);
    shader->setInt("shadowMap", 0);
    shader->setVec3("viewPos", currentCamera->Position);
    shader->setInt("numLights", static_cast<int>(lightPos.size()));

    char buf[32];
    for (int i = 0; i < (int)lightPos.size(); i++)
    {
        snprintf(buf, sizeof(buf), "lights[%d].Position", i);
        shader->setVec3(buf, lightPos[i]);
        snprintf(buf, sizeof(buf), "lights[%d].Color", i);
        shader->setVec3(buf, glm::vec3(1.0f));
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    shader->setInt("shadowMap", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, t_grassDiff);
    shader->setInt("tex_grass_diff", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, t_grassNor);
    shader->setInt("tex_grass_nor", 2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, t_stoneDiff);
    shader->setInt("tex_stone_diff", 3);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, t_stoneNor);
    shader->setInt("tex_stone_nor", 4);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, t_rock2Diff);
    shader->setInt("tex_rock2_diff", 5);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, t_rock2Nor);
    shader->setInt("tex_rock2_nor", 6);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, t_grassAO);
    shader->setInt("tex_grass_ao", 7);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, t_stoneAO);
    shader->setInt("tex_stone_ao", 8);
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, t_grassRough);
    shader->setInt("tex_grass_rough", 9);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, t_stoneRough);
    shader->setInt("tex_stone_rough", 10);
    shader->setBool("hasGrassAO", t_grassAO != 0);
    shader->setBool("hasStoneAO", t_stoneAO != 0);
    shader->setBool("hasGrassRough", t_grassRough != 0);
    shader->setBool("hasStoneRough", t_stoneRough != 0);

    for (auto &[path, instances] : perModel)
    {
        Model *model = pathToModel[path];
        if (!model || instances.empty())
            continue;

        // One-time setup: attach instance VBO to the mesh VAOs
        setupTileBatch(path, *model, instances.size() + 64);

        auto &batch = tileBatches[path];

        // Upload this frame's instance data
        glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);
        size_t needed = instances.size() * sizeof(TileInstance);
        GLint allocated;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &allocated);
        if ((size_t)allocated < needed)
            glBufferData(GL_ARRAY_BUFFER, needed, instances.data(), GL_DYNAMIC_DRAW);
        else
            glBufferSubData(GL_ARRAY_BUFFER, 0, needed, instances.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Draw all meshes of this model in one instanced call
        for (auto &mesh : model->meshes)
        {
            glBindVertexArray(mesh.VAO);
            glDrawElementsInstanced(GL_TRIANGLES,
                                    static_cast<GLsizei>(mesh.indices.size()),
                                    GL_UNSIGNED_INT, 0,
                                    static_cast<GLsizei>(instances.size()));
        }
        glBindVertexArray(0);
    }
}

void RenderManager::renderGrass(const glm::vec3 &position, float grassHeight, int grassDensity, float windStrength)
{
    ZoneScoped;

    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in render grass: " << error << std::endl;
    }
    // Generate instances if not done yet

    if (!grassModelLoaded)
    {
        try
        {
            grassModel = std::make_shared<Model>("assets/grass_4.obj");
            grassModelLoaded = true;
            std::cout << "Grass model loaded successfully" << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to load grass model: " << e.what() << std::endl;
            grassModelLoaded = false;
        }
    }
    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error rendering grass: " << error << std::endl;
    }

    Shader *grassShader = getShader("grass_shader");
    if (!grassShader)
    {
        std::cerr << "Grass shader not found!" << std::endl;
        return;
    }

    generateGrassInstances(position, 2.0f, 10.0f);
    setupGrassInstancing();

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error RENDERING grass: " << error << std::endl;
    }

    grassShader->use();

    // // Setup OpenGL state for grass rendering
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glDisable(GL_CULL_FACE);
    // glDepthMask(GL_FALSE); // Disable depth writing for transparent grass

    // Load and bind textures
    unsigned int windDistortionTexture = loadAndCacheTexture(
        "windDistortionTexture", "assets/CircleDisplacementObject.png");
    unsigned int grassMaskTexture = loadAndCacheTexture(
        "grassMaskTexture", "assets/GrassMask.png");
    unsigned int groundTexture = loadAndCacheTexture(
        "groundTexture", "assets/GroundTexture.png");

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, groundTexture);
    grassShader->setInt("groundTexture", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, windDistortionTexture);
    grassShader->setInt("windDistortionMap", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, grassMaskTexture);
    grassShader->setInt("grassMask", 2);

    if (textures.find("grass") != textures.end())
    {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, textures.at("grass")->id);
        grassShader->setInt("grassTexture", 3);
    }

    // Set transformation matrices
    // Set the view and projection matrices as before
    grassShader->setMat4("view", viewMatrix);
    grassShader->setMat4("projection", projectionMatrix);

    // 1. Initialize the model matrix to the identity matrix
    glm::mat4 model = glm::mat4(1.0f);

    // // 2. Apply a rotation
    // // Rotate the model by 'angle' radians around the Y-axis.
    // // You'll need to define or calculate 'angle' (e.g., based on time for animation).
    // float angle = glm::radians(90.0f); // Example: 45 degrees
    // model = glm::rotate(model, angle, glm::vec3(1.0f, 0.0f, 0.0f));

    // // 3. Apply a scale
    // // Scale the model by a factor of 5 in X, Y, and Z
    // model = glm::scale(model, glm::vec3(5.0f, 5.0f, 5.0f));

    // 4. Send the final model matrix to the shader    // // 2. Apply a rotation
    // // Rotate the model by 'angle' radians around the Y-axis.
    // // You'll need to define or calculate 'angle' (e.g., based on time for animation).
    // float angle = glm::radians(90.0f); // Example: 45 degrees
    // model = glm::rotate(model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
    grassShader->setFloat("fadeDistance", 80.0f);

    // Seasonal and environmental parameters
    grassShader->setFloat("seasonalTint", 0.0f); // 0 = summer, 1 = autumn
    grassShader->setFloat("healthVariation", 0.1f);
    grassShader->setFloat("dryness", 0.0f);

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in rendergrass: " << error << std::endl;
    }

    // Render instanced grass
    if (!grassInstances.empty())
    {
        // Use instanced rendering
        for (auto &mesh : grassModel->meshes)
        {
            glBindVertexArray(mesh.VAO);
            glDrawElementsInstanced(GL_TRIANGLES, mesh.indices.size(),
                                    GL_UNSIGNED_INT, 0, grassInstances.size());
        }
    }

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in rendergrass: " << error << std::endl;
    }

    // grassModel->Draw(*grassShader);
    // Restore OpenGL state
    // glDepthMask(GL_TRUE);
    // glEnable(GL_CULL_FACE);
    // glDisable(GL_BLEND);

    // Unbind textures
    for (int i = 0; i < 4; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void RenderManager::renderParabolicTrajectory(glm::vec3 start, glm::vec3 target,
                                              int segments)
{

    std::vector<float> vertexIndices;
    for (int i = 0; i <= segments; ++i)
    {
        vertexIndices.push_back(static_cast<float>(i));
    }

    static unsigned int thickVAO = 0, thickVBO = 0;
    static int cachedSegments = -1;

    if (thickVAO == 0 || cachedSegments != segments)
    {
        if (thickVAO != 0)
        {
            glDeleteVertexArrays(1, &thickVAO);
            glDeleteBuffers(1, &thickVBO);
        }

        glGenVertexArrays(1, &thickVAO);
        glGenBuffers(1, &thickVBO);

        glBindVertexArray(thickVAO);
        glBindBuffer(GL_ARRAY_BUFFER, thickVBO);
        glBufferData(GL_ARRAY_BUFFER, vertexIndices.size() * sizeof(float),
                     vertexIndices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        cachedSegments = segments;
    }

    // Use the thick line shader (load this with vertex + geometry + fragment)
    Shader *thickShader = getShader("line_shader");
    if (thickShader)
    {
        thickShader->use();

        // Your existing uniforms
        thickShader->setMat4("model", glm::mat4(1.0f));
        thickShader->setMat4("view", viewMatrix);
        thickShader->setMat4("projection", projectionMatrix);
        thickShader->setVec3("startPos", start);
        thickShader->setVec3("targetPos", target);
        thickShader->setInt("segments", segments);
        thickShader->setFloat("gravity", 1.0f);
        thickShader->setFloat("initialVelocity", 1.0f);
        thickShader->setFloat("pointSize", 1.0f);
        thickShader->setFloat("arcHeightMultiplier", 1.0f);
        thickShader->setVec3("color", glm::vec3(1.0f, 1.0f, 0.0f));
        thickShader->setFloat("alpha", 1.0f);
        thickShader->setFloat("arcFactor", 1.0f); // parabolic

        // Thickness uniforms for geometry shader
        thickShader->setFloat("lineWidth", 20.0f);
        thickShader->setVec2("screenSize", glm::vec2(screenWidth, screenHeight));
        thickShader->setBool("antiAlias", false);

        // Enable blending for smooth edges
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindVertexArray(thickVAO);
        glDrawArrays(GL_POINTS, 0, segments + 1);
        glBindVertexArray(0);

        glDisable(GL_BLEND);
    }
}

void RenderManager::renderLinearTrajectory(glm::vec3 start, glm::vec3 target, int segments)
{
    std::vector<float> vertexIndices;
    for (int i = 0; i <= segments; ++i)
    {
        vertexIndices.push_back(static_cast<float>(i));
    }

    static unsigned int linearVAO = 0, linearVBO = 0;
    static int cachedSegments = -1;

    if (linearVAO == 0 || cachedSegments != segments)
    {
        if (linearVAO != 0)
        {
            glDeleteVertexArrays(1, &linearVAO);
            glDeleteBuffers(1, &linearVBO);
        }

        glGenVertexArrays(1, &linearVAO);
        glGenBuffers(1, &linearVBO);

        glBindVertexArray(linearVAO);
        glBindBuffer(GL_ARRAY_BUFFER, linearVBO);
        glBufferData(GL_ARRAY_BUFFER, vertexIndices.size() * sizeof(float),
                     vertexIndices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        cachedSegments = segments;
    }

    Shader *thickShader = getShader("line_shader");
    if (thickShader)
    {
        thickShader->use();

        thickShader->setMat4("model", glm::mat4(1.0f));
        thickShader->setMat4("view", viewMatrix);
        thickShader->setMat4("projection", projectionMatrix);
        thickShader->setVec3("startPos", start);
        thickShader->setVec3("targetPos", target);
        thickShader->setInt("segments", segments);
        thickShader->setFloat("gravity", 0.0f);
        thickShader->setFloat("initialVelocity", 0.0f);
        thickShader->setFloat("pointSize", 1.0f);
        thickShader->setFloat("arcHeightMultiplier", 0.0f);
        thickShader->setVec3("color", glm::vec3(1.0f, 1.0f, 0.0f));
        thickShader->setFloat("alpha", 1.0f);
        thickShader->setFloat("arcFactor", 0.0f); // linear

        thickShader->setFloat("lineWidth", 20.0f);
        thickShader->setVec2("screenSize", glm::vec2(screenWidth, screenHeight));
        thickShader->setBool("antiAlias", false);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindVertexArray(linearVAO);
        glDrawArrays(GL_POINTS, 0, segments + 1);
        glBindVertexArray(0);

        glDisable(GL_BLEND);
    }
}

void RenderManager::renderArrow(glm::vec3 position)
{
    Shader *simpleShader = getShader("simple_color_shader");
    simpleShader->use();
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    simpleShader->setMat4("model", model);
    simpleShader->setMat4("view", viewMatrix);
    simpleShader->setMat4("projection", projectionMatrix);
    simpleShader->setVec3("objectColor", linesColor);
    arrowModel->Draw(*simpleShader);
}

void RenderManager::renderVerticalArrow(glm::vec3 position)
{
    Shader *simpleShader = getShader("simple_color_shader");
    simpleShader->use();
    glm::mat4 model = glm::mat4(1.0f); // Start with identity matrix
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    model = glm::scale(model, glm::vec3(0.45f)); // Shrink to 20% of original size
    simpleShader->setMat4("model", model);
    simpleShader->setMat4("view", viewMatrix);
    simpleShader->setMat4("projection", projectionMatrix);
    simpleShader->setVec3("objectColor", linesColor);
    arrowModel->Draw(*simpleShader);
}

void RenderManager::renderEl(glm::vec3 position)
{
    Shader *simpleShader = getShader("simple_color_shader");
    simpleShader->use();
    simpleShader->setMat4("model", glm::translate(glm::mat4(1.0f), position));
    simpleShader->setMat4("view", viewMatrix);
    simpleShader->setMat4("projection", projectionMatrix);
    simpleShader->setVec3("objectColor", linesColor);
    elModel->Draw(*simpleShader);
}

void RenderManager::renderLine(glm::vec3 position)
{
    Shader *simpleShader = getShader("simple_color_shader");
    simpleShader->use();
    simpleShader->setMat4("model", glm::translate(glm::mat4(1.0f), position));
    simpleShader->setMat4("view", viewMatrix);
    simpleShader->setMat4("projection", projectionMatrix);
    simpleShader->setVec3("objectColor", linesColor);
    lineModel->Draw(*simpleShader);
}

void RenderManager::setUpSkyBox()
{
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    vector<std::string> faces = {
        "assets/skybox/right.jpg",
        "assets/skybox/left.jpg",
        "assets/skybox/top.jpg",
        "assets/skybox/bottom.jpg",
        "assets/skybox/front.jpg",
        "assets/skybox/back.jpg"};
    cubemapTexture = loadCubemapForSkyBox(faces);
}

void RenderManager::renderSkyBox()
{
    Shader *skyboxShader = getShader("skybox_shader");
    skyboxShader->use();
    skyboxShader->setInt("skybox", 5);

    glDepthFunc(GL_LEQUAL); // change depth function so depth test passes when
                            // values are equal to depth buffer's content
    skyboxShader->use();
    viewMatrix = glm::mat4(glm::mat3(
        currentCamera->GetViewMatrix())); // remove translation from the view matrix
    skyboxShader->setMat4("view", viewMatrix);
    skyboxShader->setMat4("projection", projectionMatrix);
    // skybox cube
    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}

// ---------------------------------------------------------------------------
// Water mesh generation
// Creates a flat XZ grid of (divisions x divisions) quads at Y=yLevel.
// The mesh is stored in waterVAO/VBO/EBO and re-used every frame.
// ---------------------------------------------------------------------------
void RenderManager::generateWaterMesh(float halfExtent, float yLevel, int divisions)
{
    // Clean up any pre-existing mesh
    if (waterVAO != 0)
    {
        glDeleteVertexArrays(1, &waterVAO);
        glDeleteBuffers(1, &waterVBO);
        glDeleteBuffers(1, &waterEBO);
        waterVAO = waterVBO = waterEBO = waterIndexCount = 0;
    }

    waterYLevel = yLevel;
    waterHalfExtent = halfExtent;

    int vertsPerSide = divisions + 1;

    // Each vertex: position(3) + normal(3) + texcoord(2) = 8 floats
    std::vector<float> verts;
    verts.reserve(vertsPerSide * vertsPerSide * 8);

    for (int z = 0; z <= divisions; ++z)
    {
        for (int x = 0; x <= divisions; ++x)
        {
            float fx = -halfExtent + (float)x / (float)divisions * 2.0f * halfExtent;
            float fz = -halfExtent + (float)z / (float)divisions * 2.0f * halfExtent;

            // Position
            verts.push_back(fx);
            verts.push_back(yLevel);
            verts.push_back(fz);
            // Normal (pointing up; geometry shader overwrites with accurate value)
            verts.push_back(0.0f);
            verts.push_back(1.0f);
            verts.push_back(0.0f);
            // Texcoord
            verts.push_back((float)x / (float)divisions);
            verts.push_back((float)z / (float)divisions);
        }
    }

    // Two triangles per quad, wound CCW from above so normals point +Y
    std::vector<unsigned int> indices;
    indices.reserve(divisions * divisions * 6);

    for (int z = 0; z < divisions; ++z)
    {
        for (int x = 0; x < divisions; ++x)
        {
            unsigned int tl = z * vertsPerSide + x;
            unsigned int tr = tl + 1;
            unsigned int bl = tl + vertsPerSide;
            unsigned int br = bl + 1;

            // Triangle 1 — CCW from +Y
            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);
            // Triangle 2
            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    waterIndexCount = (unsigned int)indices.size();

    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);
    glGenBuffers(1, &waterEBO);

    glBindVertexArray(waterVAO);

    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    // aPos      — location 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    // aNormal   — location 1
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    // aTexCoords — location 2
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));

    glBindVertexArray(0);

    std::cout << "[Water] Mesh generated: " << divisions << "x" << divisions
              << " quads, " << waterIndexCount / 3 << " triangles, "
              << "extent=" << halfExtent << ", y=" << yLevel << std::endl;

    // Ensure textures are loaded (needed by renderWaterPass)
    if (textures.find("foam") == textures.end())
        loadTexture("foam", "assets/foam.png");
    if (textures.find("water_normal") == textures.end())
        loadTexture("water_normal", "assets/water_normal.png");
}

// ---------------------------------------------------------------------------
// Water render pass — called after all opaque objects, before skybox.
// Uses water.vs (FBM waves) + water.gs (accurate normals) + water_forward.fs
// (cubemap reflection + Fresnel + foam).
// ---------------------------------------------------------------------------
void RenderManager::renderWaterPass()
{
    if (waterVAO == 0)
        return;

    Shader *shader = getShader("water_forward");
    if (!shader)
        return;

    shader->use();

    // Matrices
    glm::mat4 model = glm::mat4(1.0f);
    shader->setMat4("model", model);
    shader->setMat4("view", viewMatrix);
    shader->setMat4("projection", projectionMatrix);
    shader->setFloat("time", (float)glfwGetTime());

    // Wave parameters (matched to level scale)
    shader->setFloat("waveHeight", 0.22f);
    shader->setFloat("waveSpeed", 0.35f);
    shader->setFloat("waveFreq", 0.45f);
    shader->setFloat("surfaceLevel", waterYLevel);
    shader->setFloat("surfaceThickness", 2.0f);

    // Camera
    shader->setVec3("cameraWorldPos", currentCamera->Position);

    // Water appearance uniforms
    shader->setVec3("waterColorShallow", glm::vec3(0.30f, 0.72f, 0.92f));
    shader->setVec3("waterColorDeep", glm::vec3(0.04f, 0.20f, 0.55f));
    shader->setFloat("waterAlpha", 0.92f); // higher = less see-through colour bleed
    shader->setFloat("reflectStrength", 0.70f);
    shader->setFloat("specStrength", 0.80f);
    shader->setFloat("foamStrength", 0.65f);

    // Sun — must match the direction baked into cubemap.fs so reflections are consistent
    glm::vec3 sun = glm::normalize(glm::vec3(0.55f, 0.30f, 0.40f));
    shader->setVec3("sunDir", sun);
    shader->setVec3("sunColor", glm::vec3(1.0f, 0.96f, 0.88f));

    // Normal map (two scrolling layers in shader)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures.at("water_normal")->id);
    shader->setInt("normalMap", 0);

    // Foam texture
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures.at("foam")->id);
    shader->setInt("foamTexture", 1);

    // Planar reflection texture + VP matrix for correct UV projection
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, reflectionTexture);
    shader->setInt("reflectionTexture", 2);
    shader->setMat4("reflectionVP", reflectionVP);

    // Enable alpha blending; disable depth writes so transparent water
    // doesn't occlude objects behind it in the depth buffer
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    // Render both sides (camera can dip below water level)
    glDisable(GL_CULL_FACE);

    glBindVertexArray(waterVAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)waterIndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

// ---------------------------------------------------------------------------
// Ground mesh — flat plane at groundYLevel, covers the visible seabed under
// the transparent water.  Vertex layout: position(3)+normal(3)+texcoord(2).
// ---------------------------------------------------------------------------
void RenderManager::generateGroundMesh(float halfExtent, float yLevel, int divisions)
{
    // Clean up any pre-existing mesh
    if (groundVAO != 0)
    {
        glDeleteVertexArrays(1, &groundVAO);
        glDeleteBuffers(1, &groundVBO);
        glDeleteBuffers(1, &groundEBO);
        groundVAO = groundVBO = groundEBO = groundIndexCount = 0;
    }

    groundYLevel = yLevel;
    groundHalfExtent = halfExtent;

    int vertsPerSide = divisions + 1;

    std::vector<float> verts;
    verts.reserve(vertsPerSide * vertsPerSide * 8);

    for (int z = 0; z <= divisions; ++z)
    {
        for (int x = 0; x <= divisions; ++x)
        {
            float fx = -halfExtent + (float)x / (float)divisions * 2.0f * halfExtent;
            float fz = -halfExtent + (float)z / (float)divisions * 2.0f * halfExtent;

            // Position
            verts.push_back(fx);
            verts.push_back(yLevel);
            verts.push_back(fz);
            // Normal (up)
            verts.push_back(0.0f);
            verts.push_back(1.0f);
            verts.push_back(0.0f);
            // Texcoord (normalised 0-1 across the whole plane; shader will tile by world pos)
            verts.push_back((float)x / (float)divisions);
            verts.push_back((float)z / (float)divisions);
        }
    }

    std::vector<unsigned int> indices;
    indices.reserve(divisions * divisions * 6);

    for (int z = 0; z < divisions; ++z)
    {
        for (int x = 0; x < divisions; ++x)
        {
            unsigned int tl = z * vertsPerSide + x;
            unsigned int tr = tl + 1;
            unsigned int bl = tl + vertsPerSide;
            unsigned int br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);
            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    groundIndexCount = (unsigned int)indices.size();

    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glGenBuffers(1, &groundEBO);

    glBindVertexArray(groundVAO);

    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));

    glBindVertexArray(0);

    std::cout << "[Ground] Mesh generated: " << divisions << "x" << divisions
              << " quads, extent=" << halfExtent << ", y=" << yLevel << std::endl;

    if (textures.find("ground_tex") == textures.end())
        loadTexture("ground_tex", "assets/GroundTexture.png");
}

// ---------------------------------------------------------------------------
// Ground render pass — called before water so depth test hides it where land
// tiles cover it, and the water sees it through its transparency.
// ---------------------------------------------------------------------------
void RenderManager::renderGroundPass()
{
    if (groundVAO == 0)
        return;

    Shader *shader = getShader("ground_shader");
    if (!shader)
        return;

    shader->use();

    // Matrices
    glm::mat4 model = glm::mat4(1.0f);
    shader->setMat4("model", model);
    shader->setMat4("view", viewMatrix);
    shader->setMat4("projection", projectionMatrix);
    shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

    // Camera / lighting
    if (currentCamera)
        shader->setVec3("viewPos", currentCamera->Position);

    if (!lightPositions.empty())
    {
        shader->setVec3("lightPos", lightPositions[0]);
        shader->setVec3("lightColor", glm::vec3(1.0f, 0.95f, 0.85f));
    }

    // shader->setFloat("texTiling", 4.0f); // repeat texture every 4 world units

    // Bind ground texture
    auto it = textures.find("ground_tex");
    if (it != textures.end())
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, it->second->id);
        shader->setInt("groundTexture", 0);
    }

    // Bind shadow map
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    shader->setInt("shadowMap", 1);

    glBindVertexArray(groundVAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)groundIndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Procedural terrain — domain-warped FBM height field, flat-normal toon shade
// ---------------------------------------------------------------------------

// ---- Noise helpers (file-local, not exposed in header) --------------------

static float ptHash(float x, float y)
{
    // Fast deterministic hash in [0,1)
    float h = std::sin(x * 127.1f + y * 311.7f) * 43758.5453123f;
    return h - std::floor(h);
}

static float ptNoise(float x, float y)
{
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - (float)ix, fy = y - (float)iy;
    // Smoothstep
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    float a = ptHash((float)ix, (float)iy);
    float b = ptHash((float)(ix + 1), (float)iy);
    float c = ptHash((float)ix, (float)(iy + 1));
    float d = ptHash((float)(ix + 1), (float)(iy + 1));
    return glm::mix(glm::mix(a, b, ux), glm::mix(c, d, ux), uy);
}

static float ptFBM(float x, float y, int octaves)
{
    float val = 0.0f, amp = 0.5f, freq = 1.0f;
    for (int i = 0; i < octaves; ++i)
    {
        val += amp * ptNoise(x * freq, y * freq);
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return val;
}

// Domain-warped FBM — "state of the art" terrain: warp input coords with a
// secondary FBM to break up grid artifacts and produce rocky, overhanging forms.
static float ptDomainWarpedFBM(float x, float y)
{
    float wx = ptFBM(x + 1.7f, y + 9.2f, 4);
    float wy = ptFBM(x + 8.3f, y + 2.8f, 4);
    const float warpStrength = 2.8f;
    return ptFBM(x + warpStrength * wx, y + warpStrength * wy, 7);
}

static float ptHeight(float worldX, float worldZ, float amplitude)
{
    const float freq = 0.025f; // lower → larger feature scale
    float raw = ptDomainWarpedFBM(worldX * freq, worldZ * freq);
    // raw in [0,1] approx; remap to [0, amplitude]
    return raw * amplitude;
}

// ---- Mesh generation ------------------------------------------------------
void RenderManager::generateProcTerrainMesh(float halfExtentX, float halfExtentZ, float baseY, int divisions, float centerX, float centerZ)
{
    if (procTerrainVAO != 0)
    {
        glDeleteVertexArrays(1, &procTerrainVAO);
        glDeleteBuffers(1, &procTerrainVBO);
        glDeleteBuffers(1, &procTerrainEBO);
        procTerrainVAO = procTerrainVBO = procTerrainEBO = procTerrainIndexCount = 0;
    }

    const float amplitude = 10.0f; // max height above baseY
    int vertsPerSide = divisions + 1;

    // Each vertex: position(3) + normal(3) + texcoord(2) = 8 floats
    // Normals start as (0,1,0); the geometry shader overwrites with flat normals.
    std::vector<float> verts;
    verts.reserve(vertsPerSide * vertsPerSide * 8);

    for (int z = 0; z <= divisions; ++z)
    {
        for (int x = 0; x <= divisions; ++x)
        {
            float fx = -halfExtentX + (float)x / (float)divisions * 2.0f * halfExtentX;
            float fz = -halfExtentZ + (float)z / (float)divisions * 2.0f * halfExtentZ;
            float fy = baseY + ptHeight(fx, fz, amplitude);

            verts.push_back(fx);
            verts.push_back(fy);
            verts.push_back(fz);
            // Normal placeholder (GS computes actual flat normals)
            verts.push_back(0.0f);
            verts.push_back(1.0f);
            verts.push_back(0.0f);
            // Texcoord
            verts.push_back((float)x / (float)divisions);
            verts.push_back((float)z / (float)divisions);
        }
    }

    std::vector<unsigned int> indices;
    indices.reserve(divisions * divisions * 6);

    for (int z = 0; z < divisions; ++z)
    {
        for (int x = 0; x < divisions; ++x)
        {
            unsigned int tl = z * vertsPerSide + x;
            unsigned int tr = tl + 1;
            unsigned int bl = tl + vertsPerSide;
            unsigned int br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    procTerrainIndexCount = (unsigned int)indices.size();

    glGenVertexArrays(1, &procTerrainVAO);
    glGenBuffers(1, &procTerrainVBO);
    glGenBuffers(1, &procTerrainEBO);

    glBindVertexArray(procTerrainVAO);

    glBindBuffer(GL_ARRAY_BUFFER, procTerrainVBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, procTerrainEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));

    glBindVertexArray(0);

    std::cout << "[ProcTerrain] Mesh generated: " << divisions << "x" << divisions
              << " quads, " << procTerrainIndexCount / 3 << " triangles"
              << ", extent=" << halfExtentX << ", baseY=" << baseY << std::endl;
}

// ---- Render pass -----------------------------------------------------------
void RenderManager::renderProcTerrainPass()
{
    if (procTerrainVAO == 0)
        return;

    Shader *shader = getShader("proc_terrain");
    if (!shader)
        return;

    shader->use();

    glm::mat4 model = glm::mat4(1.0f);
    shader->setMat4("model", model);
    shader->setMat4("view", viewMatrix);
    shader->setMat4("projection", projectionMatrix);

    if (currentCamera)
        shader->setVec3("viewPos", currentCamera->Position);

    if (!lightPositions.empty())
    {
        shader->setVec3("lightPos", lightPositions[0]);
        shader->setVec3("lightColor", glm::vec3(1.0f, 0.95f, 0.85f));
    }
    else
    {
        shader->setVec3("lightPos", glm::vec3(0.0f, 50.0f, 0.0f));
        shader->setVec3("lightColor", glm::vec3(1.0f, 0.95f, 0.85f));
    }

    glBindVertexArray(procTerrainVAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)procTerrainIndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// setupReflectionFBO — creates a half-resolution colour+depth FBO used by
// renderReflectionPass() to capture the scene from the mirrored camera.
// ---------------------------------------------------------------------------
void RenderManager::setupReflectionFBO()
{
    // Clean up existing resources
    if (reflectionFBO)
    {
        glDeleteFramebuffers(1, &reflectionFBO);
        reflectionFBO = 0;
    }
    if (reflectionTexture)
    {
        glDeleteTextures(1, &reflectionTexture);
        reflectionTexture = 0;
    }
    if (reflectionDepthRBO)
    {
        glDeleteRenderbuffers(1, &reflectionDepthRBO);
        reflectionDepthRBO = 0;
    }

    int w = std::max(screenWidth / 2, 1);
    int h = std::max(screenHeight / 2, 1);

    glGenFramebuffers(1, &reflectionFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);

    // Colour attachment
    glGenTextures(1, &reflectionTexture);
    glBindTexture(GL_TEXTURE_2D, reflectionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reflectionTexture, 0);

    // Depth renderbuffer
    glGenRenderbuffers(1, &reflectionDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, reflectionDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflectionDepthRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[Reflection] Framebuffer incomplete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::cout << "[Reflection] FBO created " << w << "x" << h << std::endl;
}

// ---------------------------------------------------------------------------
// renderReflectionPass — renders all opaque scene objects from a camera
// mirrored across the water surface, into reflectionFBO.  The resulting
// texture is sampled by water_forward.fs for planar reflections.
// ---------------------------------------------------------------------------
void RenderManager::renderReflectionPass()
{
    if (!reflectionFBO || !currentCamera || !currentScene)
        return;

    int w = std::max(screenWidth / 2, 1);
    int h = std::max(screenHeight / 2, 1);

    // -- 1. Build reflected view matrix ------------------------------------
    // Mirror the camera across the horizontal plane Y = waterYLevel.
    glm::vec3 camPos = currentCamera->Position;
    float wy = waterYLevel;
    glm::vec3 reflPos = glm::vec3(camPos.x, 2.0f * wy - camPos.y, camPos.z);
    glm::vec3 camTarget = camPos + currentCamera->Front;
    glm::vec3 reflTarget = glm::vec3(camTarget.x, 2.0f * wy - camTarget.y, camTarget.z);
    // Keep Up direction unchanged — reflected camera is below looking upward;
    // flipping Up is NOT needed because projecting through reflectionVP already
    // accounts for the mirror, and the Y-flip in lookAt would flip the FBO image.
    glm::vec3 reflUp = currentCamera->Up;
    glm::mat4 reflView = glm::lookAt(reflPos, reflTarget, reflUp);

    // Save reflectionVP for the water shader to project fragments correctly
    reflectionVP = projectionMatrix * reflView;

    // -- 2. Bind FBO + set half-res viewport --------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);
    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // -- 3. Enable clip plane: keep only geometry above the water surface ---
    // Plane equation:  dot((x,y,z,1), (0,1,0,-wy)) = y - wy >= 0
    activeClipPlane = glm::vec4(0.0f, 1.0f, 0.0f, -wy);
    glEnable(GL_CLIP_DISTANCE0);

    // Disable backface culling so tile/model undersides are visible in reflection
    glDisable(GL_CULL_FACE);

    // -- 4. Override view matrix so useShader() uses the reflected camera ---
    glm::mat4 savedView = viewMatrix;
    viewMatrix = reflView;

    // -- 5. Render all opaque scene objects ---------------------------------
    std::vector<glm::vec3> lightPos;
    for (auto &l : lights)
        lightPos.push_back(l.position);

    // Terrain tiles use instanced rendering — skip individual draw + batch them
    renderTilesInstanced(currentScene->getGameObjects(), lightPos, lightSpaceMatrix);

    for (auto &obj : currentScene->getGameObjects())
    {
        if (obj->shaderName == "terrain_tile_shader")
            continue;
        if (obj->name == "dune")
            continue;
        renderGameObject(*obj, lightPos, lightSpaceMatrix);
    }

    // -- 6. Restore state ---------------------------------------------------
    viewMatrix = savedView;
    glDisable(GL_CLIP_DISTANCE0);
    glEnable(GL_CULL_FACE);
    activeClipPlane = glm::vec4(0.0f, -1.0f, 0.0f, 1e6f);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenWidth, screenHeight);
}

void RenderManager::prepareAllAnimations()
{
    ZoneScoped;
    float t = (float)glfwGetTime();
    // NOTE: BeginFrame() is called in main.cpp AFTER renderShadowPass(), so the
    // shadow pass reads last frame's cached bone matrices (animated, 1-frame latency).
    // This function only launches the async jobs for THIS frame's bone computation.

    // Collect unique model pointers that have skeletal animation data.
    // Multiple GameObjects can share the same Model (e.g. instanced characters).
    std::vector<Model*> toUpdate;
    {
        std::unordered_set<Model*> seen;
        for (auto& go : currentScene->getGameObjects())
        {
            if (!go || !go->model || !go->model->IsAnimated()) continue;
            Model* m = go->model.get();
            if (seen.insert(m).second)
                toUpdate.push_back(m);
        }
    }

    // One std::async job per model — returns immediately, jobs run in background.
    // renderMainPass() waits for completion before any skinned draw.
    m_animFutures.clear();
    m_animFutures.reserve(toUpdate.size());
    for (Model* m : toUpdate)
        m_animFutures.push_back(std::async(std::launch::async,
                                           [m, t]{ m->PrepareAnimation(t); }));
    // Returns immediately.
}

void RenderManager::renderShadowPass()
{
    ZoneScoped;
    if (lightPositions.empty())
        return;

    // WHY single light: the fragment shaders only call ShadowCalculation for i==0,
    // so running the full depth pass for every light in lightPositions was rendering
    // N-1 complete shadow maps that are never read — pure waste.
    const glm::vec3 &lightPos = lightPositions[0];

    glm::mat4 lightProjection = glm::ortho(-25.0f, 25.0f, -25.0f, 25.0f, near_plane, far_plane);
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    lightSpaceMatrix = lightProjection * lightView;

    // WHY pointer: `Shader depthPrePass = *getShader(...)` copied the entire Shader
    // struct by value every frame, then renderGameObjectWithShader took it by value
    // again — another copy per object. A pointer costs nothing.
    Shader *depthShader = getShader("depth_pre_pass");
    depthShader->use();
    depthShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    // Disable face culling for the shadow pass. Without this, the pass inherits
    // whatever cull state the previous frame left — non-deterministic. More
    // importantly, back-face culling from the light's PoV would discard faces
    // that should write depth (e.g. the underside of a mesh that faces the light
    // at a low angle), leaving holes in the shadow map.
    glDisable(GL_CULL_FACE);

    for (auto &i : currentScene->getGameObjects())
    {
        // Light-space frustum cull: skip objects outside the shadow map region.
        // We do NOT camera-cull here — see drawShadowCaster for explanation.
        if (isInLightFrustum(i->position, 2.0f, lightProjection, lightView))
            drawShadowCaster(*i, depthShader);
    }

    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderManager::renderMainPass()
{
    ZoneScoped;

    // Block until all PrepareAnimation() jobs from prepareAllAnimations() finish.
    // They've been running in parallel with renderShadowPass() — by now most are done.
    {
        ZoneScopedN("WaitForAnimations");
        for (auto& f : m_animFutures) f.get();
        m_animFutures.clear();
    }

    // Build the planar reflection texture before any objects write to the main
    // depth buffer — the reflection pass needs a clean depth buffer.
    renderReflectionPass();

    auto gameObjects = currentScene->getGameObjects();

    // Debug: Log object count every 60 frames
    static int frameCounter = 0;
    if (frameCounter++ % 60 == 0)
    {
        // std::cout << "[DEBUG] Main pass rendering " << gameObjects.size() << " objects" << std::endl;
    }

    // Cache the animated light position calculation (was being calculated 3 times per object!)
    // float animatedLightOffset = static_cast<float>(sin(glfwGetTime() * 1.5) * 3.0);
    // glm::vec3 lightPos = glm::vec3(animatedLightOffset, animatedLightOffset, animatedLightOffset);
    std::vector<glm::vec3> lightPositions;
    for (auto light : lights)
    {
        lightPositions.push_back(light.position);
    }

    // Draw all terrain_tile_shader objects in one instanced batch per model
    renderTilesInstanced(gameObjects, lightPositions, lightSpaceMatrix);

    for (auto &i : gameObjects)
    {
        // Terrain tiles are handled by renderTilesInstanced above
        if (i->shaderName == "terrain_tile_shader")
            continue;

        if (i->name == "dune")
        {
            // renderGameObjectWithShader(*i, *getShader("dune_shader"));
            continue;
        }

        // Find if this object is a game entity
        std::shared_ptr<GameEntity> entity = nullptr;
        if (gameInstance)
        {
            for (auto &e : currentScene->getGameEntities())
            {
                if (e->object->ID == i->ID)
                {
                    entity = e;
                    break;
                }
            }
        }

        // Always render the actual object at its current position
        renderGameObject(*i, lightPositions, lightSpaceMatrix);

        // Render ghost at destination if entity has queued movements (EXECUTING) or buffered MOVE (PLANNING)
        if (entity)
        {
            glm::vec3 destination = glm::vec3(0.0f);
            bool hasDestination = false;

            if (entity->hasQueuedMovements())
            {
                destination = entity->getQueuedDestination();
                hasDestination = true;
            }
            else
            {
                for (const auto &action : entity->getActionBuffer())
                {
                    if (action.type == ActionType::MOVE)
                    {
                        destination = action.targetPosition;
                        hasDestination = true;
                        break;
                    }
                }
            }

            if (hasDestination && glm::distance(destination, entity->object->position) > 0.5f)
            {
                renderGhostObject(*entity->object, destination, 0.4f);
            }
        }

        // Visual feedback for entities
        if (gameInstance && entity && gameInstance->isEntitySelectable(entity))
        {
            // Highlight selected entity with gold outline
            if (gameInstance->getSelectedEntity() && entity == gameInstance->getSelectedEntity())
            {
                for (auto &tilePosition : getReachableTilesForEntity(entity))
                {
                    renderSelectedTile(tilePosition, glm::vec3(0.9, 0.8, 0.3), 0.05f, 0.80f);
                }
            }
            // Show entities that have already moved with red tint
            else if (entity->hasMovedThisTurn)
            {
                // renderSelectedTile(i->position + glm::vec3(1, 1, 1), glm::vec3(0.9, 0.8, 0.3), 0.05f, 0.80f);
            }
            // Show entities that can still move with green tint
            else
            {
                // renderSelectedTile(i->position, glm::vec3(0.9, 0.8, 0.3), 0.05f, 0.80f);
                // renderSelectedTile(i->position + glm::vec3(1, 0, 1), glm::vec3(0.9, 0.8, 0.3), 0.05f, 0.80f);
                // renderSelectedTile(i->position - glm::vec3(1, 0, 1), glm::vec3(0.9, 0.8, 0.3), 0.05f, 0.80f);
            }
        }

        // Highlight reachable tiles when in Move mode
        if (i->name.find("cube") != std::string::npos &&
            gameInstance &&
            gameInstance->getSelectedEntity() &&
            gameInstance->canEntityMove(gameInstance->getSelectedEntity()) &&
            gameInstance->getSelectedEntity()->isReachable(i->position) &&
            uiManager &&
            uiManager->isCharacterMoving)
        {
            renderSelectedTile(i->position, glm::vec3(0.1, 0.1, 0.9), 0.02f, 0.60f);
        }
    }

    // ID buffer rendering — only update on mouse click, not every frame
    if (needIDBufferUpdate)
    {
        renderSceneToIDBuffer(gameObjects);
        needIDBufferUpdate = false;
    }

    currentScene->renderCompactColorPicker();

    // Trajectory rendering (toggleable for performance)
    if (renderTrajectory)
    {
        renderParabolicTrajectory(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(-2.5f, 2.5f, 0.66f), trajectorySegments);
    }

    // Pass trajectory preview - show when entity has a buffered PASS action
    if (uiManager && gameInstance)
    {
        for (auto &entity : gameInstance->getScene()->getGameEntities())
        {
            if (entity->getHasBall())
            {
                // Check if this entity has a buffered PASS action
                for (const auto &action : entity->getActionBuffer())
                {
                    if (action.type == ActionType::PASS && action.targetEntity)
                    {
                        renderLinearTrajectory(
                            entity->object->position + glm::vec3(0.0f, 0.5f, 0.0f),
                            action.targetEntity->object->position + glm::vec3(0.0f, 0.5f, 0.0f),
                            100);
                        break;
                    }
                }
                break;
            }
        }
    }

    // if (gameInstance->getGameMode() == ENGINE)
    // {
    //     for (auto &light : lights)
    //         renderArrow(light.position);
    // }

    // Ground pass first so it writes depth; water (alpha-blended) sees it beneath.
    renderGroundPass();

    // Render water after all opaque objects so depth test works correctly,
    // and before skybox so the sky cubemap is already bound for reflection.
    renderWaterPass();

    renderSkyBox();
}

// ---------------------------------------------------------------------------
// Rain system
// ---------------------------------------------------------------------------

void RenderManager::initRainSystem()
{
    rainParticles.resize(RAIN_COUNT);
    rainInstanceData.resize(RAIN_COUNT);

    // Fixed world-space box, matching renderRainPass constants.
    std::uniform_real_distribution<float> xDist(-80.0f, 80.0f);
    std::uniform_real_distribution<float> zDist(-80.0f, 80.0f);
    std::uniform_real_distribution<float> yDist(-10.0f, 30.0f);

    for (auto &p : rainParticles)
    {
        p.pos = glm::vec3(xDist(rainRng), yDist(rainRng), zDist(rainRng));
        rainInstanceData[&p - rainParticles.data()] = p.pos;
    }

    // Static quad mesh: a thin elongated strip oriented in local space.
    // x in [-0.5, 0.5], y in [0.0, 1.0] — mapped to width/length in the shader.
    glm::vec2 meshVerts[4] = {
        {-0.5f, 0.0f}, // bottom-left
        {0.5f, 0.0f},  // bottom-right
        {-0.5f, 1.0f}, // top-left
        {0.5f, 1.0f},  // top-right
    };

    glGenVertexArrays(1, &rainVAO);
    glGenBuffers(1, &rainMeshVBO);
    glGenBuffers(1, &rainInstanceVBO);

    glBindVertexArray(rainVAO);

    // Attribute 0: per-vertex local XY
    glBindBuffer(GL_ARRAY_BUFFER, rainMeshVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(meshVerts), meshVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void *)0);
    glEnableVertexAttribArray(0);

    // Attribute 2: per-instance world position
    glBindBuffer(GL_ARRAY_BUFFER, rainInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, RAIN_COUNT * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);
}

void RenderManager::renderRainPass(float dt)
{
    if (!rainEnabled || !currentCamera || rainVAO == 0)
        return;

    constexpr float fallSpeed = 20.0f;
    constexpr float halfTileX = 35.0f; // tiling half-extent around camera
    constexpr float halfTileZ = 35.0f;
    constexpr float worldYTop = 30.0f;
    constexpr float worldYBot = -10.0f;

    const glm::vec3 camPos = currentCamera->Position;

    // Wrap helper: maps value into [lo, hi) with modular arithmetic
    auto wrap = [](float x, float lo, float hi) -> float
    {
        float range = hi - lo;
        return x - range * std::floor((x - lo) / range);
    };

    std::uniform_real_distribution<float> ySpawnDist(worldYBot, worldYTop);

    for (int i = 0; i < RAIN_COUNT; ++i)
    {
        rainParticles[i].pos.y -= fallSpeed * dt;

        // Wrap XZ so rain tiles infinitely around the camera.
        // Particles leaving one edge reappear on the opposite edge —
        // always behind the camera so the seam is never visible.
        rainParticles[i].pos.x = wrap(rainParticles[i].pos.x,
                                      camPos.x - halfTileX,
                                      camPos.x + halfTileX);
        rainParticles[i].pos.z = wrap(rainParticles[i].pos.z,
                                      camPos.z - halfTileZ,
                                      camPos.z + halfTileZ);

        // Only respawn in Y — XZ wrapping keeps them in the box forever
        if (rainParticles[i].pos.y < worldYBot)
            rainParticles[i].pos.y = ySpawnDist(rainRng);

        rainInstanceData[i] = rainParticles[i].pos;
    }

    // Upload updated positions
    glBindBuffer(GL_ARRAY_BUFFER, rainInstanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, RAIN_COUNT * sizeof(glm::vec3), rainInstanceData.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // --- Draw ---
    Shader &shader = *shaders.at("rain_shader");
    shader.use();
    shader.setMat4("view", viewMatrix);
    shader.setMat4("projection", projectionMatrix);
    shader.setVec3("cameraRight", currentCamera->Right);
    shader.setFloat("streakWidth", 0.02f);
    shader.setFloat("streakLength", 2.5f);
    shader.setFloat("opacity", 0.6f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);   // transparent — don't pollute the depth buffer
    glEnable(GL_DEPTH_TEST); // but respect depth so rain is occluded by geometry
    glDisable(GL_CULL_FACE); // rain quads must be visible from any camera angle

    glBindVertexArray(rainVAO);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, RAIN_COUNT);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

// ---------------------------------------------------------------------------

void RenderManager::sceneBuffersSetup()
{
    unsigned int sceneFramebuffer;
    unsigned int sceneColorTexture;
    unsigned int sceneDepthTexture;
    glGenFramebuffers(1, &sceneFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);

    // Create color texture
    glGenTextures(1, &sceneColorTexture);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screenWidth, screenHeight,
                 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           sceneColorTexture, 0);

    // Create depth texture (for depth testing during scene rendering)
    glGenTextures(1, &sceneDepthTexture);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, screenWidth,
                 screenHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           sceneDepthTexture, 0);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "ERROR: Scene framebuffer not complete!" << std::endl;
    }
}

std::vector<glm::vec3> RenderManager::getReachableTilesForEntity(std::shared_ptr<GameEntity> entity)
{
    std::vector<glm::vec3> reachableTiles;
    if (!entity)
        return reachableTiles;

    auto gameObjects = currentScene->getGameObjects();
    for (auto &obj : gameObjects)
    {
        if (obj->name.find("cube") != std::string::npos)
        {
            if (entity->isReachable(obj->position))
            {
                reachableTiles.push_back(obj->position);
            }
        }
    }
    return reachableTiles;
}