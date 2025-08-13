#include "render_manager.h"

RenderManager::RenderManager()
    : currentCamera(nullptr)
    , clearColor(0.2f, 0.3f, 0.3f, 1.0f)
    , wireframeMode(false)
    , depthTestEnabled(true)
    , blendingEnabled(false)
    , screenWidth(800)
    , screenHeight(600)
    , drawCalls(0)
    , verticesRendered(0)
    , ambientLight(0.1f, 0.1f, 0.1f)
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
    screenHeight = height;
    screenWidth = width;
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

void RenderManager::submit(Mesh* mesh, Shader* shader, const glm::mat4& modelMatrix, const std::vector<Texture*>& textures)
{
    // TODO: Add to opaque queue
}

void RenderManager::submitUI(Mesh* mesh, Shader* shader, const glm::mat4& modelMatrix)
{
    // TODO: Add to UI queue
}

void RenderManager::submitTransparent(Mesh* mesh, Shader* shader, const glm::mat4& modelMatrix, const std::vector<Texture*>& textures)
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

Shader* RenderManager::loadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath)
{
    // TODO: Load and compile shader
    return nullptr;
}

unsigned int RenderManager::loadTexture(const std::string& name, const char* path)
{
    auto it = textures.find(name);
    if (it != textures.end()) {
        return it->second->id;
    }
    
    auto tex = std::make_shared<Texture>(path);
    
    textures.emplace(name, tex);
    return tex->id;
}

Mesh* RenderManager::loadMesh(const std::string& name, const std::string& path)
{
    // TODO: Load mesh from file
    return nullptr;
}

Shader* RenderManager::getShader(const std::string& name)
{
    // TODO: Return shader by name
    return nullptr;
}

Texture* RenderManager::getTexture(const std::string& name)
{
    // TODO: Return texture by name
    return nullptr;
}

Mesh* RenderManager::getMesh(const std::string& name)
{
    // TODO: Return mesh by name
    return nullptr;
}

void RenderManager::setCamera(Camera* camera)
{
    // TODO: Set current camera
    currentCamera = camera;
}

void RenderManager::updateCameraMatrices()
{
    // TODO: Update view and projection matrices from camera
}

void RenderManager::addLight(const Light& light)
{
    // TODO: Add light to lights vector
}

void RenderManager::clearLights()
{
    // TODO: Clear all lights
    lights.clear();
}

void RenderManager::setAmbientLight(const glm::vec3& color)
{
    // TODO: Set ambient light color
    ambientLight = color;
}

void RenderManager::updateLightUniforms(Shader* shader)
{
    // TODO: Update light uniforms in shader
}

void RenderManager::setClearColor(const glm::vec4& color)
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

void RenderManager::renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{
    // TODO: Immediate mode line rendering
}

void RenderManager::renderWireCube(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color)
{
    // TODO: Immediate mode wire cube rendering
}

void RenderManager::renderSphere(const glm::vec3& center, float radius, const glm::vec3& color)
{
    // TODO: Immediate mode sphere rendering
}

void RenderManager::bindTextures(const std::vector<Texture*>& textures)
{
    // TODO: Bind textures to texture units
}

void RenderManager::unbindTextures()
{
    // TODO: Unbind all textures
}

void RenderManager::setupShaderUniforms(Shader* shader, const glm::mat4& modelMatrix)
{
    // TODO: Set common shader uniforms (MVP matrices, lights, etc.)
}

float RenderManager::calculateDistance(const glm::vec3& position)
{
    // TODO: Calculate distance from camera position
    return 0.0f;
}

void RenderManager::setupGBuffer() {
    if (gBufferInitialized) {
        cleanupGBuffer(); // Clean up existing G-Buffer first
    }
    
    // Generate framebuffer
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    
    createGBufferTextures();
    
    // Tell OpenGL which color attachments we'll use for rendering 
    unsigned int attachments[6] = { 
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 
    };
    glDrawBuffers(6, attachments);
    
    // Check framebuffer completeness
    if (!checkGBufferStatus()) {
        std::cerr << "ERROR::FRAMEBUFFER:: G-buffer is not complete!" << std::endl;
        return;
    }
    
    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gBufferInitialized = true;
    
    std::cout << "G-Buffer setup complete!" << std::endl;
}

void RenderManager::createGBufferTextures() {
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

bool RenderManager::checkGBufferStatus() {
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        switch(status) {
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

void RenderManager::cleanupGBuffer() {
    if (gBufferInitialized) {
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

void RenderManager::bindGBuffer() {
    if (gBufferInitialized) {
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
        glViewport(0, 0, screenWidth, screenHeight);
    }
}

void RenderManager::unbindGBuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderManager::resizeGBuffer(int width, int height) {
    if (width != screenWidth || height != screenHeight) {
        screenWidth = width;
        screenHeight = height;
        if (gBufferInitialized) {
            setupGBuffer(); // Recreate G-Buffer with new dimensions
        }
    }
}

void RenderManager::setupMSAAGBuffer(int samples) {
    if (msaaGBufferInitialized) {
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
        GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 
    };
    glDrawBuffers(6, msaaAttachments);
    
    // Check framebuffer completeness
    if (!checkMSAAGBufferStatus()) {
        std::cerr << "ERROR::FRAMEBUFFER:: MSAA G-buffer is not complete!" << std::endl;
        return;
    }
    
    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    msaaGBufferInitialized = true;
    
    std::cout << "MSAA G-Buffer setup complete with " << samples << " samples!" << std::endl;
}

void RenderManager::createMSAAGBufferTextures() {
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

bool RenderManager::checkMSAAGBufferStatus() {
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        switch(status) {
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

void RenderManager::cleanupMSAAGBuffer() {
    if (msaaGBufferInitialized) {
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

void RenderManager::bindMSAAGBuffer() {
    if (msaaGBufferInitialized) {
        glBindFramebuffer(GL_FRAMEBUFFER, msaaGBuffer);
        glViewport(0, 0, screenWidth, screenHeight);
    }
}

void RenderManager::unbindMSAAGBuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderManager::resolveMSAAGBuffer() {
    if (!msaaGBufferInitialized || !gBufferInitialized) {
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

void RenderManager::resizeMSAAGBuffer(int width, int height) {
    if (width != screenWidth || height != screenHeight) {
        screenWidth = width;
        screenHeight = height;
        if (msaaGBufferInitialized) {
            setupMSAAGBuffer(msaaSamples); // Recreate MSAA G-Buffer with new dimensions
        }
    }
}

// Skybox setup function
void RenderManager::setupSkybox()
{
    if (skyboxInitialized) {
        cleanupSkybox(); // Clean up existing skybox first
    }
    
    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
    
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    glBindVertexArray(0);
    
    skyboxInitialized = true;
    std::cout << "Skybox setup complete!" << std::endl;
}

// Skybox cleanup function
void RenderManager::cleanupSkybox()
{
    if (skyboxInitialized) {
        glDeleteVertexArrays(1, &skyboxVAO);
        glDeleteBuffers(1, &skyboxVBO);
        skyboxVAO = 0;
        skyboxVBO = 0;
        skyboxInitialized = false;
    }
}

// Set skybox texture
void RenderManager::setSkyboxTexture(unsigned int texture)
{
    skyboxTexture = texture;
}

// Set skybox shader
void RenderManager::setSkyboxShader(Shader* shader)
{
    skyboxShader = shader;
}

// Render skybox function
void RenderManager::renderSkybox()
{
    if (!skyboxInitialized || !skyboxShader || skyboxTexture == 0 || !currentCamera) {
        return;
    }
    
    // Change depth function so depth test passes when values are equal to depth buffer's content
    glDepthFunc(GL_LEQUAL);
    
    skyboxShader->use();
    
    // Remove translation from the view matrix
    glm::mat4 view = glm::mat4(glm::mat3(currentCamera->GetViewMatrix()));
    skyboxShader->setMat4("view", view);
    skyboxShader->setMat4("projection", projectionMatrix);
    
    // Skybox cube
    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    // Reset depth function
    glDepthFunc(GL_LESS);
    
    drawCalls++;
    verticesRendered += 36;
}

// Load cubemap texture
unsigned int RenderManager::loadCubemap(const std::vector<std::string>& faces)
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
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}


void RenderManager::renderLine(glm::vec3 rayOrigin, glm::vec3 rayDir, glm::mat4 view, float thickness = 0.1f, float length = 0.1f)
{
    unsigned int lineVAO = 0;
    unsigned int lineVBO = 0;   
    if (lineVAO == 0) {
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
    if (glm::length(offset1) < 1e-6f || glm::length(offset2) < 1e-6f) {
        // Fallback: use camera up vector if cross product fails
        offset1 = camRight * (thickness * 0.5f);
        offset2 = camUp * (thickness * 0.5f);
    }
    
    // Create 8 vertices for a rectangular tube (4 at start, 4 at end)
    glm::vec3 startVerts[4] = {
        rayOrigin + offset1 + offset2,  // top-right
        rayOrigin - offset1 + offset2,  // top-left
        rayOrigin - offset1 - offset2,  // bottom-left
        rayOrigin + offset1 - offset2   // bottom-right
    };
    
    glm::vec3 endVerts[4] = {
        rayEnd + offset1 + offset2,     // top-right
        rayEnd - offset1 + offset2,     // top-left
        rayEnd - offset1 - offset2,     // bottom-left
        rayEnd + offset1 - offset2      // bottom-right
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
        startVerts[2].x, startVerts[2].y, startVerts[2].z
    };
    
    // Upload vertex data
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    // Setup vertex attributes (location 0 = vec3 position)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glDisable(GL_CULL_FACE);
    
    // Render the rectangular tube (24 vertices = 8 triangles)
    glDrawArrays(GL_TRIANGLES, 0, 24);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
}

void RenderManager::renderQuad()
{

    unsigned int quadVAO = 0;
    unsigned int quadVBO;
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}