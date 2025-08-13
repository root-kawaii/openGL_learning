#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <../src/shader_m.h>
#include <../src/camera.h>
#include <../src/model.h>

#include <../src/input.h>

#include <../src/game_object.h>
// #include <../src/scene.h>

#include <iostream>
#include <../json/single_include/nlohmann/json.hpp>
#include <filesystem>

#include <chrono>
#include <thread>

#include <../src/raycast.h>

// #include "../src/texture_debugger.cpp"
#include "../src/game.h"
#include "../src/texture.h"

unsigned int loadCubemap(vector<std::string> faces);
GameObject loadSceneObject(const std::string& path, int stride, unsigned int textureID);
void shaderUser(Shader& shader, glm::mat4 *projection, glm::mat4 *model,  glm::mat4 *view,  glm::vec3 *cameraPos);

// settings
bool shadows = true;

bool selected = false;
int selectedID = 0;

int entityCounter = 0;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// meshes
unsigned int planeVAO;

glm::vec3 lightPos(-1.0f, 1.0f, 10.0f);

int main()
{
    Game game = Game();


    Scene mainScene = Scene();
    RenderManager renderManager = game.getRenderManager();
    renderManager.initialize(game.SCR_WIDTH, game.SCR_HEIGHT);
    renderManager.setupGBuffer();
    renderManager.setupMSAAGBuffer();

    namespace fs = std::filesystem;
    // unsigned int albedo = loadTexture(fs::path("assets/cerberus/Textures/rusted_iron/Cerberus_A.tga").c_str());
    unsigned int texture_metallic = renderManager.loadTexture("texture_metallic", fs::path("assets/cerberus/Textures/metallic.png").c_str());
    // unsigned int normal = loadTexture(fs::path("assets/cerberus/Textures/rusted_iron/Cerberus_N.tga").c_str());
    unsigned int texture_roughness = renderManager.loadTexture("texture_roughness", fs::path("assets/cerberus/Textures/roughness.png").c_str());

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);


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


    // build and compile shaders
    // -------------------------
    // Shader shader("3.2.blending.vs", "3.2.blending.fs");

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------

    // cubemap
    vector<std::string> faces = 
    {
        "assets/skybox/right.jpg",
        "assets/skybox/left.jpg",
        "assets/skybox/top.jpg",
        "assets/skybox/bottom.jpg",
        "assets/skybox/front.jpg",
        "assets/skybox/back.jpg"
    };
    unsigned int cubemapTexture = loadCubemap(faces);  

    Shader waterShader("shaders/water.vs", "shaders/water.fs");
    Shader terrainShader("shaders/g_buffer_2.vs", "shaders/g_buffer.fs");
    Shader shaderGeometryPass("shaders/g_buffer.vs", "shaders/g_buffer.fs");
    Shader selectedShader("shaders/g_buffer.vs", "shaders/g_buffer_selected.fs");
    Shader shaderLightingPass("shaders/deferred_shading.vs", "shaders/deferred_shading.fs");
    Shader shaderLightBox("shaders/deferred_light_box.vs", "shaders/deferred_light_box.fs");
    Shader skyboxShader("shaders/cubemap.vs", "shaders/cubemap.fs");
    Shader simpleDepthShader("shaders/simple_depth_shader.vs", "shaders/simple_depth_shader.fs", "shaders/simple_depth_shader.gs");
    Shader simpleShader("shaders/shader.vs", "shaders/shader.fs");
    Shader debugShader("shaders/debug.vs", "shaders/debug.fs");
    Shader modelShader("shaders/model.vs", "shaders/model.fs");

    // Shader selectedShader("shaders/selected_shader.vs", "shaders/selected_shader.fs");


    Model backpack(fs::path("assets/backpack/backpack.obj"));
    Model plane(fs::path("assets/planes/plane_2.obj"));

    auto gun = std::make_shared<GameObject>("gun_1", "assets/cerberus/cerberus.glb", glm::vec3(0.0f, 3.0f, 0.0f));
    auto ball = std::make_shared<GameObject>("ball_1", "assets/ball_2.obj", glm::vec3(0.0f, 3.0f, 0.0f));

    GameObject* ballPtr = ball.get();  // Get raw pointer before moving
    GameObject* gunPtr = gun.get();  // Get raw pointer before moving

    // Add to scene
    mainScene.addGameObject(gun);
    mainScene.addGameObject(ball);


    Model helmet(fs::path("assets/helmet.glb"));


    std::vector<glm::vec3> objectPositions;
    objectPositions.push_back(glm::vec3(-3.0,  -0.5, -3.0));
    objectPositions.push_back(glm::vec3( 3.0,  -0.5, 3.0));
    objectPositions.push_back(glm::vec3( -0.5,  -0.5,  0.0));
    objectPositions.push_back(glm::vec3( 8.0,  0.5,  3.0));



    unsigned int sceneFramebuffer;
    unsigned int sceneColorTexture;
    unsigned int sceneDepthTexture;
    glGenFramebuffers(1, &sceneFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
    
    // Create color texture
    glGenTextures(1, &sceneColorTexture);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, game.SCR_WIDTH, game.SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);
    
    // Create depth texture (for depth testing during scene rendering)
    glGenTextures(1, &sceneDepthTexture);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, game.SCR_WIDTH, game.SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTexture, 0);
    
    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "ERROR: Scene framebuffer not complete!" << std::endl;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // lighting info
    // -------------
    const unsigned int NR_LIGHTS = 32;
    std::vector<glm::vec3> lightPositions;
    std::vector<glm::vec3> lightColors;
    srand(13);
    for (unsigned int i = 0; i < NR_LIGHTS; i++)
    {
        // calculate slightly random offsets
        float xPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 3.0);
        float yPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 4.0);
        float zPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 3.0);
        lightPositions.push_back(glm::vec3(xPos, yPos, zPos));
        // also calculate random color
        float rColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
        float gColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
        float bColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
        lightColors.push_back(glm::vec3(rColor, gColor, bColor));
    }


    // cubemaps VAO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
      


    unsigned int framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);


    // shadowmaps

    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    // create depth cubemap texture
    unsigned int depthCubemap;
    glGenTextures(1, &depthCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
    for (unsigned int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // attach depth texture as FBO's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);



    shaderLightingPass.use();
    shaderLightingPass.setInt("gPosition", 0);
    shaderLightingPass.setInt("gNormal", 1);
    shaderLightingPass.setInt("gAlbedoSpec", 2);
    shaderLightingPass.setInt("gDepth", 3);
    shaderLightingPass.setInt("gLinearDepth", 4);
    shaderLightingPass.setInt("gMetallic", 5);
    shaderLightingPass.setInt("gRoughness", 6);
    shaderLightingPass.setInt("depthMap", 7);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 5);


    // render loop
    // -----------
    Ray ray;
    glm::mat4 viewCopy; 
    bool viewFrozen = false;
    bool intersect = false;

    while (!glfwWindowShouldClose(game.getWindow()))
    {

        if(shadows) {
            // std::cout << "msaa enabled" << std::endl;
            glEnable(GL_MULTISAMPLE);
        }
        else{
            // std::cout << "msaa disabled" <<std::endl;
            glDisable(GL_MULTISAMPLE);
        }


        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        // std::cout << 1/deltaTime << std::endl;
        lastFrame = currentFrame;

        lightPos.z = static_cast<float>(sin(glfwGetTime() * 1.5) * 3.0);
        // input
        // -----
        processInput(game.getWindow(), &game.camera, deltaTime, shadows, game.seed);

        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // shadows
        float near_plane = 0.10f;
        float far_plane = 100.0f;
        glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), (float)SHADOW_WIDTH / (float)SHADOW_HEIGHT, near_plane, far_plane);
        std::vector<glm::mat4> shadowTransforms;
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        
        

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glm::mat4 model;
        simpleDepthShader.use();
        for (unsigned int i = 0; i < 6; ++i)
            simpleDepthShader.setMat4("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
        simpleDepthShader.setFloat("far_plane", far_plane);
        simpleDepthShader.setVec3("lightPos", lightPos);
        // renderScene(simpleDepthShader);
        for (unsigned int i = 0; i < objectPositions.size(); i++)
            {
                model = glm::mat4(1.0f);
                model = glm::translate(model, objectPositions[i]);
                model = glm::scale(model, glm::vec3(1.0f));
                simpleDepthShader.setMat4("model", model);
                ballPtr->model.Draw(simpleDepthShader);


            }

        // model = glm::mat4(1.0f); 
        // model = glm::translate(model, glm::vec3( 0.0,  -2.0,  0.0));
        // simpleDepthShader.setMat4("model", model);
        // plane.Draw(simpleDepthShader);


        // model = glm::mat4(1.0f); 
        // model = glm::translate(model, glm::vec3(15.5f,-2.5f,0.5f));
        // simpleDepthShader.setMat4("model", model);
        // plane.Draw(simpleDepthShader);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);



        // 1. geometry pass: render scene's geometry/color data into gbuffer
        // -----------------------------------------------------------------
        glViewport(0, 0, game.SCR_WIDTH, game.SCR_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, renderManager.getGBuffer());
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glm::mat4 projection = glm::perspective(glm::radians(game.camera.Zoom), (float)game.SCR_WIDTH / (float)game.SCR_HEIGHT, near_plane, far_plane);
        glm::mat4 view = game.camera.GetViewMatrix();
        model = glm::mat4(1.0f);

        

        glm::mat4 frozenView; // outside render loop
        bool viewFrozen;


        if (glfwGetKey(game.getWindow(), GLFW_KEY_Q) == GLFW_PRESS) {
            ray = screenToWorldRay(glm::vec2(game.SCR_WIDTH / 2.0f, game.SCR_HEIGHT / 2.0f),
                                    game.camera, game.SCR_WIDTH, game.SCR_HEIGHT, projection);
            bool intersect = rayIntersectMesh(ray, ballPtr->model.meshes);
            selected = true;
            frozenView = game.camera.GetViewMatrix(); // 
            viewFrozen = true;
            selectedID = ballPtr->ID;
            


            if (intersect) {
                std::cout << "intersect" << std::endl;
                // selected = true;
                // frozenView = camera.GetViewMatrix();
                // viewFrozen = true;
            } else {
                std::cout << "no intersection" << std::endl;
                // selected = false;
                // viewFrozen = false;
            }
        }


        modelShader.use();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);
        modelShader.setMat4("model", model);
 
        if (selected && viewFrozen) {
            std::cout << "lining" << std::endl;
            renderManager.renderLine(ray.origin, ray.direction, frozenView, 0.01f, 1000.0f);
        }

        if (true) {
            std::cout << "moving" << std::endl;
            if (glfwGetKey(game.getWindow(), GLFW_KEY_UP) == GLFW_PRESS && glfwGetKey(game.getWindow(), GLFW_KEY_LEFT_SHIFT) != GLFW_PRESS){
                ballPtr->position.x += 1;
            }
            if (glfwGetKey(game.getWindow(), GLFW_KEY_DOWN) == GLFW_PRESS && glfwGetKey(game.getWindow(), GLFW_KEY_LEFT_SHIFT) != GLFW_PRESS){
                ballPtr->position.x -= 1;
            }
            if (glfwGetKey(game.getWindow(), GLFW_KEY_RIGHT) == GLFW_PRESS){
                ballPtr->position.z += 1;
            }
            if (glfwGetKey(game.getWindow(), GLFW_KEY_LEFT) == GLFW_PRESS){
                ballPtr->position.z -= 1;
            }
            if (glfwGetKey(game.getWindow(), GLFW_KEY_UP) == GLFW_PRESS && glfwGetKey(game.getWindow(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS){
                ballPtr->position.y += 1;
            }
            if (glfwGetKey(game.getWindow(), GLFW_KEY_DOWN) == GLFW_PRESS && glfwGetKey(game.getWindow(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS){
                ballPtr->position.y -= 1;
            }
            
        }


        shaderGeometryPass.use();

        shaderGeometryPass.setInt("texture_metallic",6);
        shaderGeometryPass.setInt("texture_roughness",7);


        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, texture_metallic);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, texture_roughness);


        shaderGeometryPass.setMat4("projection", projection);
        shaderGeometryPass.setMat4("view", view);
        shaderGeometryPass.setFloat("near_plane", near_plane);  // Add this
        shaderGeometryPass.setFloat("far_plane", far_plane); // Add this
        for (unsigned int i = 0; i < objectPositions.size(); i++)
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, objectPositions[i]);
            model = glm::scale(model, glm::vec3(0.5f));
            shaderGeometryPass.setMat4("model", model);
            ballPtr->model.Draw(shaderGeometryPass);
            model = glm::translate(model, glm::vec3( 0.0,  -2.0,  0.0));
            shaderGeometryPass.setMat4("model", model);

            // model = glm::translate(model, glm::vec3(15.5f,-2.5f,0.5f));
            // shaderGeometryPass.setMat4("model", model);
            // plane.Draw(shaderGeometryPass);
        }

        // plane.Draw(shaderGeometryPass);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3( 0.0,  -2.0,  0.0));
        shaderGeometryPass.setMat4("model", model);
        plane.Draw(shaderGeometryPass);


        selectedShader.use();


        std::cout << selectedID << std::endl;
        std::cout << ballPtr->ID << std::endl;

        shaderGeometryPass.use();
        model = glm::mat4(1.0f);
        model = glm::translate(model, ballPtr->position);
        shaderGeometryPass.setMat4("projection", projection);
        shaderGeometryPass.setMat4("view", view);
        shaderGeometryPass.setMat4("model", model);
        shaderGeometryPass.setFloat("time", glfwGetTime());
        shaderGeometryPass.setFloat("selected", selectedID);
        shaderGeometryPass.setFloat("objectID", ballPtr->ID);
        ballPtr->model.Draw(shaderGeometryPass);
        



        shaderGeometryPass.setFloat("objectID", 0);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3( 9.0,  -1.0, lightPos.z));
        model = glm::scale(model, glm::vec3(0.3f));
        shaderGeometryPass.setMat4("model", model);
        backpack.Draw(shaderGeometryPass);


        model = glm::mat4(1.0f);
        glm::vec3 gunOffset = glm::vec3(0.05f, -0.05f, -0.3f); // Adjust these values for desired position
        model = glm::translate(model, gunOffset);
        glm::mat3 cameraRotationInverse = glm::transpose(glm::mat3(game.camera.GetViewMatrix()));
        model = glm::mat4(cameraRotationInverse) * model; // Apply camera's rotation to the gun
        model = glm::translate(glm::mat4(1.0f), game.camera.Position) * model;
        model = glm::scale(model, glm::vec3(0.001f)); // Keep your original scale
        shaderGeometryPass.setMat4("model", model);
        gunPtr->model.Draw(shaderGeometryPass);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3( 0.0,  3.0, 0.0));
        model = glm::scale(model, glm::vec3(0.5f));
        shaderGeometryPass.setMat4("model", model);
        helmet.Draw(shaderGeometryPass);
        

        terrainShader.use();
        terrainShader.setFloat("seed", game.seed);
        terrainShader.setMat4("projection", projection);
        terrainShader.setMat4("view", view);
        terrainShader.setFloat("near_plane", near_plane);  // Add thiss
        terrainShader.setFloat("far_plane", far_plane); // Add this

        model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(1000.0f));
        model = glm::translate(model, glm::vec3( 25.0,  -5.0,  25.0));
        terrainShader.setMat4("model", model);
        plane.Draw(terrainShader);


        // 2. lighting pass: calculate lighting by iterating over a screen filled quad pixel-by-pixel using the gbuffer's content.
        // -----------------------------------------------------------------------------------------------------------------------

    

        glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
        glViewport(0, 0, game.SCR_WIDTH, game.SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT );



        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderManager.getGBuffer());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
        // blit to default framebuffer. Note that this may or may not work as the internal formats of both the FBO and default framebuffer have to match.
        // the internal formats are implementation defined. This works on all of my systems, but if it doesn't on yours you'll likely have to write to the 		
        // depth buffer in another shader stage (or somehow see to match the default framebuffer's internal format with the FBO's internal format).
        glBlitFramebuffer(0, 0, game.SCR_WIDTH, game.SCR_HEIGHT, 0, 0, game.SCR_WIDTH, game.SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);

        shaderLightingPass.use();
        // shadows
        // glCullFace(GL_FRONT);
        // PBR
        shaderLightingPass.setFloat("metallic", 0.1f);
        shaderLightingPass.setFloat("roughness", 0.2f);
        // shaderLightingPass.setFloat("metallic", static_cast<float>(sin(glfwGetTime() * 1.5) ));
        // shaderLightingPass.setFloat("roughness", static_cast<float>(sin(glfwGetTime())));
        shaderLightingPass.setFloat("ao", 0.5f);
        ///
        shaderLightingPass.setMat4("projection", projection);
        shaderLightingPass.setMat4("view", view);
        // set lighting uniformss
        shaderLightingPass.setVec3("viewPos", game.camera.Position);
        shaderLightingPass.setInt("shadows", 1); // enable/disable shadows by pressing 'SPACE'
        shaderLightingPass.setFloat("far_plane", far_plane);
        shaderLightingPass.setFloat("near_plane", near_plane);
        // NEW: Set the missing uniforms for improved shader
        shaderLightingPass.setInt("numLights", lightPositions.size());
        shaderLightingPass.setFloat("ambientStrength", 0.1f);
        shaderLightingPass.setFloat("shadowBias", 0.05f);
        // send light relevant uniforms
        for (unsigned int i = 0; i < lightPositions.size(); i++)
        {
            shaderLightingPass.setVec3("lights[" + std::to_string(i) + "].Position", lightPositions[i]);
            shaderLightingPass.setVec3("lights[" + std::to_string(i) + "].Color", lightColors[i]);
            // update attenuation parameters and calculate radius
            const float linear = 0.7f;
            const float quadratic = 1.8f;
            shaderLightingPass.setFloat("lights[" + std::to_string(i) + "].Linear", linear);
            shaderLightingPass.setFloat("lights[" + std::to_string(i) + "].Quadratic", quadratic);
            float maxBrightness = std::fmaxf(std::fmaxf(lightColors[i].r, lightColors[i].g), lightColors[i].b);
            float radius = (-linear + std::sqrt(linear * linear - 4 * quadratic * (1.0f - (256.0f / 5.0f) * maxBrightness))) / (2 * quadratic);
            shaderLightingPass.setFloat("lights[" + std::to_string(i) + "].Radius", radius);
        }



        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderManager.getPositionTexture());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderManager.getNormalTexture());
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, renderManager.getAlbedoSpecTexture());
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, renderManager.getDepthTexture());
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, renderManager.getLinearDepthTexture());
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, renderManager.getMetallicTexture());
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, renderManager.getRoughnessTexture());
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

        // finally render quad
        glDisable(GL_DEPTH_TEST);
        renderManager.renderQuad();
        glEnable(GL_DEPTH_TEST);


        // //////////////////////////////////////

        glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFramebuffer);

        // Bind the default framebuffer for drawing
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default framebuffer

        // Copy color buffer to the screen
        glBlitFramebuffer(
            0, 0, game.SCR_WIDTH, game.SCR_HEIGHT,    // src rect
            0, 0, game.SCR_WIDTH, game.SCR_HEIGHT,    // dst rect
            GL_COLOR_BUFFER_BIT,            // what to copy
            GL_NEAREST                      // filtering
        );

        // ///////////
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

        // glDisable(GL_DEPTH_TEST); // Disable depth testing for post-processing
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Reflections


        waterShader.use();
        waterShader.setFloat("time", glfwGetTime());
        waterShader.setFloat("waveHeight", 0.75f);
        waterShader.setFloat("waveSpeed", 0.3f);
        waterShader.setFloat("waveFreq", 0.3f);

        // Set up model matrix (if your water is a simple plane transformed by 'model')
        // This is typically done per object.
        model = glm::mat4(1.0f);
        // model = glm::translate(model, glm::vec3(5.5f, -2.5f, 0.5f)); // Your water position
        waterShader.setMat4("model", model);
        waterShader.setMat4("view", view);  

        // --- Pass all necessary view/projection matrices ---
        // These are standard for any object rendering, and also needed by the fragment shader
        waterShader.setMat4("projection", projection); // 'projection' uniform in FS
        waterShader.setMat4("viewMatrix", view);      // 'viewMatrix' uniform in FS (was 'view' in VS, consistent name for FS)

        // Calculate and pass the combined and inverse matrices
        glm::mat4 viewProjectionMatrix = projection * view;
        waterShader.setMat4("viewProjection", viewProjectionMatrix);         // 'viewProjection' uniform in FS
        waterShader.setMat4("inverseViewProjection", glm::inverse(viewProjectionMatrix)); // 'inverseViewProjection' uniform in FS

        // --- CRITICAL ADDITIONS from our corrected fragment shader ---
        // These two were missing from your latest C++ snippet, but are crucial for ReconstructWorldPosition:
        waterShader.setMat4("inverseProjection", glm::inverse(projection)); // NEW: 'inverseProjection' uniform in FS
        waterShader.setMat4("inverseView", glm::inverse(view));             // NEW: 'inverseView' uniform in FS

        // Set camera position
        waterShader.setVec3("cameraWorldPos", game.camera.Position);

        // Set screen size
        waterShader.setVec2("screenSize", glm::vec2(game.SCR_WIDTH, game.SCR_HEIGHT));

        // --- ADDED: Near and Far Plane values ---
        // These are now uniforms in the fragment shader for LinearizeDepth (even if not directly used by current ReconstructWorldPosition)
        // Make sure 'yourCameraNearPlane' and 'yourCameraFarPlane' are actual float values from your camera setup
        // For example: camera.NearPlane, camera.FarPlane, or hardcoded floats like 0.1f, 100.0f
        waterShader.setFloat("nearPlane", near_plane); // e.g., camera.nearPlane
        waterShader.setFloat("farPlane", far_plane);   // e.g., camera.farPlane

        // Bind G-buffer textures
        while (glGetError() != GL_NO_ERROR);

        // Bind textures with error checking
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderManager.getPositionTexture());
        if (glGetError() != GL_NO_ERROR) std::cout << "Error binding gPosition" << std::endl;

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderManager.getNormalTexture());
        if (glGetError() != GL_NO_ERROR) std::cout << "Error binding gNormal" << std::endl;

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, renderManager.getAlbedoSpecTexture());
        if (glGetError() != GL_NO_ERROR) std::cout << "Error binding gAlbedoSpec" << std::endl;

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, renderManager.getLinearDepthTexture());
        if (glGetError() != GL_NO_ERROR) std::cout << "Error binding gLinearDepth" << std::endl;

        // Set uniforms AFTER binding textures
        waterShader.setInt("gPosition", 0);
        waterShader.setInt("gNormal", 1);
        waterShader.setInt("gAlbedoSpec", 2);
        waterShader.setInt("gLinearDepth", 3);

        // Now render the water
        model = glm::translate(model, glm::vec3(5.5f, -1.75f, 0.5f)); // Your water position
        waterShader.setMat4("model", model);
        plane.Draw(waterShader);


        // Light boxes


        shaderLightBox.use();
        shaderLightBox.setMat4("projection", projection);
        shaderLightBox.setMat4("view", view);
        for (unsigned int i = 0; i < lightPositions.size(); i++)
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, lightPositions[i]);
            model = glm::scale(model, glm::vec3(0.125f));
            shaderLightBox.setMat4("model", model);
            shaderLightBox.setVec3("lightColor", lightColors[i]);
            renderManager.renderCube();
        }
        model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.325f));
        shaderLightBox.setMat4("model", model);
        renderManager.renderCube();        

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST); // Re-enable depth testing



        ///////////

        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(game.camera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);


        glfwSwapBuffers(game.getWindow());
        glfwPollEvents();

    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    // glDeleteVertexArrays(1, &planeVAO);
    // glDeleteBuffers(1, &planeVBO);
    // glDeleteFramebuffers(1, &fbo);  


    glfwTerminate();
    return 0;
}



void shaderUser(Shader& shader, glm::mat4 *projection, glm::mat4 *model,  glm::mat4 *view,  glm::vec3 *cameraPos) {
    shader.use();
    if(model) shader.setMat4("model", *model);
    if(view) shader.setMat4("view", *view);
    if(projection) shader.setMat4("projection", *projection);
    if(cameraPos) shader.setVec3("cameraPos", *cameraPos);
}



unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                         0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
            );
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap tex failed to load at path: " << faces[i] << std::endl;
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

// Object loadSceneObject(const std::string& path, int stride, unsigned int textureID) {
//     std::ifstream file(path);
//     nlohmann::json data;
//     file >> data;

//     std::vector<float> vertexBuffer;

//     for (const auto& obj : data) {
//         for (const auto& vertex : obj["vertices"]) {
//             glm::vec3 pos(
//                 vertex["position"][0],
//                 vertex["position"][1],
//                 vertex["position"][2]
//             );
        
//             glm::vec2 texPos(
//                 vertex["texcoord"][0],
//                 vertex["texcoord"][1]
//             );

//         glm::vec3 rot(obj["rotation"][0], obj["rotation"][1], obj["rotation"][2]);
//         glm::vec3 scale(obj["scale"][0], obj["scale"][1], obj["scale"][2]);

//         // Build model matrix
//         glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
//         model = glm::rotate(model, glm::radians(rot.x), glm::vec3(1, 0, 0));
//         model = glm::rotate(model, glm::radians(rot.y), glm::vec3(0, 1, 0));
//         model = glm::rotate(model, glm::radians(rot.z), glm::vec3(0, 0, 1));
//         model = glm::scale(model, scale);


//         glm::vec4 localPos(pos,1.0f);
//         glm::vec4 worldPos = model * localPos;

//         // Push transformed position
//         vertexBuffer.push_back(worldPos.x);
//         vertexBuffer.push_back(worldPos.y);
//         vertexBuffer.push_back(worldPos.z);

//         // Push texcoords (unchanged)
//         vertexBuffer.push_back(texPos.x);
//         vertexBuffer.push_back(texPos.y);


//         // std::cout << std::endl;
//         // std::cout << vertexBuffer[vertexBuffer.size() - 5];
//         // std::cout << vertexBuffer[vertexBuffer.size() - 4];
//         // std::cout << vertexBuffer[vertexBuffer.size() - 3];
//         // std::cout << vertexBuffer[vertexBuffer.size() - 2];
//         // std::cout << vertexBuffer[vertexBuffer.size() - 1];
//         // std::cout << std::endl;
        
//     }
// }


//     // Allocate heap memory to return a float* (for Object constructor)
//     size_t vertexSize = vertexBuffer.size() * sizeof(float);
//     float* buffer = new float[vertexBuffer.size()];
//     std::copy(vertexBuffer.begin(), vertexBuffer.end(), buffer);

//     return Object(buffer, vertexSize,stride, textureID);
// }





