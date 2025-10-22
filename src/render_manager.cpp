#include "render_manager.h"
#include "../tracy/public/tracy/Tracy.hpp"

namespace fs = std::filesystem;

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
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

RenderManager::RenderManager()
    : currentCamera(nullptr), clearColor(0.2f, 0.3f, 0.3f, 1.0f), wireframeMode(false), depthTestEnabled(true), blendingEnabled(false), screenWidth(800), screenHeight(600), drawCalls(0), verticesRendered(0), ambientLight(0.1f, 0.1f, 0.1f)
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
    setRes(height, width);
    setupIDBuffer();
    arrowModel = std::make_shared<Model>("assets/arrow.obj");
    lineModel = std::make_shared<Model>("assets/line.obj");
    elModel = std::make_shared<Model>("assets/el.obj");
    setUpSkyBox();
    return true;
}

void RenderManager::cleanup()
{
    // TODO: Clean up resources
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

void RenderManager::renderSphere(const glm::vec3 &center, float radius, const glm::vec3 &color)
{
    // TODO: Immediate mode sphere rendering
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
    glDepthMask(GL_FALSE);

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
    glDepthMask(GL_TRUE);
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
    shader.use();
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 scaling = glm::scale(glm::mat4(1.0f), gameObject.scale);
    model = glm::translate(model, gameObject.position) * scaling;
    shader.setMat4("projection", projectionMatrix);
    shader.setMat4("view", viewMatrix);
    shader.setMat4("model", model);
    shader.setFloat("time", glfwGetTime());
    gameObject.model.Draw(shader);
}

void RenderManager::renderGameObjectWithShader(GameObject &gameObject, Shader shader, glm::mat4 newProjectionMatrix, glm::mat4 newViewMatrix, glm::mat4 newModel)
{
    ZoneScoped;
    shader.use();
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 scaling = glm::scale(glm::mat4(1.0f), gameObject.scale);
    model = glm::translate(model, gameObject.position) * scaling;
    shader.setMat4("projection", newProjectionMatrix);
    shader.setMat4("view", newViewMatrix);
    shader.setMat4("model", model);
    shader.setFloat("time", glfwGetTime());
    gameObject.model.Draw(shader);
}

void RenderManager::renderGameObject(GameObject &gameObject, glm::vec3 lightPos, glm::mat4 lightMatrix)
{
    ZoneScoped;
    Shader shader = *getShader(gameObject.shaderName);
    useShader(gameObject, &shader, lightPos, lightMatrix);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    gameObject.model.Draw(shader);
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
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 scaling = glm::scale(glm::mat4(1.0f), gameObject->scale);
        model = glm::translate(model, gameObject->position) * scaling;
        idShader->setUint("objectID", gameObject->ID);
        idShader->setMat4("model", model);

        // Draw the object's mesh
        gameObject->model.Draw(*idShader);
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

void RenderManager::useShader(GameObject &gameObject, Shader *shader, glm::vec3 lightPos, glm::mat4 lightSpaceMatrix)
{
    glm::mat4 model = gameObject.GetTransform(); // Just use GetTransform() for consistency
    shader->use();
    shader->setMat4("projection", projectionMatrix);
    shader->setMat4("view", viewMatrix);
    shader->setMat4("model", model);
    shader->setFloat("time", glfwGetTime());
    shader->setVec3("objectColor", gameObject.color);
    shader->setVec3("viewPos", currentCamera->Position);
    shader->setVec3("lightPos", lightPos);
    shader->setVec3("lightColor", glm::vec3(1.0f, 0.0f, 0.0f));
    shader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
    shader->setInt("shadowMap", 0);

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
    gameObject.model.Draw(shader);

    // Disable blending after drawing
    if (color.a < 1.0f)
    {
        glDisable(GL_BLEND);
    }
}

void RenderManager::renderGameObjectWithTexture(GameObject &gameObject, Shader shader, unsigned int textureID)
{
    ZoneScoped;
    shader.use();
    // Check if texture actually bound
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 scaling = glm::scale(glm::mat4(1.0f), gameObject.scale);
    model = glm::translate(model, gameObject.position) * scaling;
    shader.setMat4("projection", projectionMatrix);
    shader.setMat4("view", viewMatrix);
    shader.setMat4("model", model);
    shader.setFloat("time", glfwGetTime());
    gameObject.model.SetDiffuseTexture(textureID);
    gameObject.model.Draw(shader);
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

    gameObject.model.Draw(shader);
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
    shaders["debug_shader"] = std::make_shared<Shader>("shaders/debug.vs", "shaders/debug.fs");
    shaders["model_shader"] = std::make_shared<Shader>("shaders/model.vs", "shaders/model.fs");
    shaders["smoke_shader"] = std::make_shared<Shader>("shaders/smoke.vs", "shaders/smoke.fs");
    shaders["grid_shader"] = std::make_shared<Shader>("shaders/grid.vs", "shaders/grid.fs");
    shaders["grid_shader_2"] = std::make_shared<Shader>("shaders/grid_2.vs", "shaders/grid_2.fs");
    shaders["water_noG"] = std::make_shared<Shader>("shaders/water_2.vs", "shaders/water_2.fs");
    shaders["id_shader"] = std::make_shared<Shader>("shaders/id_shader.vs", "shaders/id_shader.fs");
    shaders["line_shader"] = std::make_shared<Shader>("shaders/line_shader.vs", "shaders/line_shader.fs", "shaders/line_shader.gs");
    shaders["tile_shader"] = std::make_shared<Shader>("shaders/shader.vs", "shaders/tile_shader.fs");

    // Three-file shaders (vertex + fragment + geometry)
    shaders["simple_depth_shader"] = std::make_shared<Shader>("shaders/simple_depth_shader.vs",
                                                              "shaders/simple_depth_shader.fs",
                                                              "shaders/simple_depth_shader.gs");
    shaders["grass_shader"] = std::make_shared<Shader>("shaders/grass.vs",
                                                       "shaders/grass.fs");
    //    "shaders/grass.gs");
}

void RenderManager::initializeDepthFBO()
{
    // Create depth framebuffer
    glGenFramebuffers(1, &depthFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);

    // Create depth texture
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, screenWidth, screenHeight,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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

void RenderManager::renderGrass(const glm::vec3 &position, float grassHeight, int grassDensity, float windStrength)
{
    ZoneScoped;

    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in renderSceneToIDBuffer: " << error << std::endl;
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
        std::cerr << "OpenGL error in renderSceneToIDBuffer: " << error << std::endl;
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
        std::cerr << "OpenGL error in renderSceneToIDBuffer: " << error << std::endl;
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
    float totalTime = 2.0f * 1.0f * sin(45.0f) / 1.0f;

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

        // Thickness uniforms for geometry shader
        thickShader->setFloat("lineWidth", 200.0f);
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
        "assets/blue.png", "assets/blue.png",
        "assets/blue.png", "assets/blue.png",
        "assets/blue.png", "assets/blue.png"};
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

void RenderManager::renderShadowPass()
{
    // 1. render depth of scene to texture (from light's perspective)
    // --------------------------------------------------------------
    static glm::mat4 lightProjection, lightView;

    for (auto lightPos : lightPositions)
    {
        lightPos.z = static_cast<float>(sin(glfwGetTime() * 1.5) * 3.0);
        // lightProjection = glm::perspective(glm::radians(45.0f), (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // note that if you use a perspective projection matrix you'll have to change the light position as the current light position isn't enough to reflect the whole scene
        lightProjection = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, near_plane, far_plane);
        lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        lightSpaceMatrix = lightProjection * lightView;
        // render scene from light's point of view
        Shader depthPrePass = *getShader("depth_pre_pass");
        depthPrePass.use();
        depthPrePass.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        auto gameObjects = currentScene->getGameObjects();
        for (auto &i : gameObjects)
        {
            renderGameObjectWithShader(*i, depthPrePass, lightProjection, lightView, i->getModelMatrix());
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderManager::renderMainPass()
{

    auto gameObjects = currentScene->getGameObjects();
    gameObjects = currentScene->getGameObjects();
    for (auto &i : gameObjects)
    {
        // for (const auto lightPos : lightPositions)
        // {
        renderGameObject(*i, glm::vec3(static_cast<float>(sin(glfwGetTime() * 1.5) * 3.0), static_cast<float>(sin(glfwGetTime() * 1.5) * 3.0), static_cast<float>(sin(glfwGetTime() * 1.5) * 3.0)), lightSpaceMatrix);
        //}

        // if (i->name.find("cube") != std::string::npos && activeGameEntity.isReachable(i->position) && uiManager->isCharacterMoving)
        //     renderManager->renderSelectedTile(i->position, glm::vec3(0.1, 0.1, 0.9), 0.02f, 0.60f);
    }

    renderSceneToIDBuffer(gameObjects);
    currentScene->renderCompactColorPicker();
    int seg = 1000;
    renderParabolicTrajectory(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(-2.5f, 2.5f, 0.66f), seg);

    renderSkyBox();
}

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