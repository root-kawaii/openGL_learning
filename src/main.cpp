// OpenGL headers - MUST be first
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// OpenAL headers
#include <AL/al.h>
#include <AL/alc.h>
#include <sndfile.h>

// ImGui headers - before ImGuizmo
#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <ImGuizmo/ImGuizmo.h>

// GLM headers
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp>

// Standard library headers
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

// Third-party libraries
#include <../json/single_include/nlohmann/json.hpp>

// Tracy profiler
#include "../tracy/public/tracy/Tracy.hpp"
#include "../tracy/public/tracy/TracyOpenGL.hpp"

// Project headers
#include "../src/audio_manager.h"
#include "../src/game.h"
#include "../src/level_editor.h"
#include "../src/sphere_collision.h"
#include "../src/texture.h"
#include "../src/ui.h"
#include <../src/camera.h>
#include <../src/game_object.h>
#include <../src/input.h>
#include <../src/model.h>
#include <../src/raycast.h>
#include <../src/shader_m.h>

// Note: Uncomment these if needed
// #include <../src/scene.h>
// #include "../src/texture_debugger.cpp"

#include <random>
#include <vector>

std::vector<glm::vec3> makeThousandVecs()
{
  std::vector<glm::vec3> vecs;
  vecs.reserve(10000);

  // Random engine for variation
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(-1.75f, 1.75f); // ±0.05 variation

  for (int i = 0; i < 10000; ++i)
  {
    glm::vec3 base(0.0f, 0.5f, 0.0f);
    glm::vec3 variation(dist(gen), 0, dist(gen));
    vecs.push_back(base + variation);
  }

  return vecs;
}

unsigned int loadCubemap(vector<std::string> faces);
GameObject loadSceneObject(const std::string &path, int stride,
                           unsigned int textureID);

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

// fired
bool fired = false;

glm::vec3 lightPos(-1.0f, 1.0f, 10.0f);

struct SmokeQuad
{
  glm::vec3 position;
  float distanceToCamera;
};

int main()
{

  std::shared_ptr<Game> game = std::make_shared<Game>();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(game->getWindow(), true);
  ImGui_ImplOpenGL3_Init("#version 330");

  auto ui = std::make_shared<UIManager>(game->SCR_HEIGHT, game->SCR_WIDTH);
  auto mainScene = std::make_shared<Scene>();
  game->setScene(mainScene);

  // Get pointers to managers (assuming they're already heap-allocated in Game)
  RenderManager *renderManager = &game->getRenderManager();
  AudioManager *audioManager = &game->getAudioManager();

  auto levelEditor = std::make_shared<LevelEditor>();
  levelEditor->setRenderManager(renderManager);
  levelEditor->setGame(game);

  renderManager->setCamera(&game->camera);
  renderManager->initialize(game->SCR_WIDTH, game->SCR_HEIGHT);
  renderManager->setupGBuffer();
  renderManager->setupMSAAGBuffer();
  renderManager->initializeShaders();
  renderManager->initializeDepthFBO();

  game->getScene()->setRenderManager(renderManager);

  namespace fs = std::filesystem;
  // unsigned int albedo =
  // loadTexture(fs::path("assets/cerberus/Textures/rusted_iron/Cerberus_A.tga").c_str());
  // unsigned int texture_metallic = renderManager->loadTexture(
  //     "texture_metallic",
  //     fs::path("assets/cerberus/Textures/metallic.png").c_str());
  // // unsigned int normal =
  // // loadTexture(fs::path("assets/cerberus/Textures/rusted_iron/Cerberus_N.tga").c_str());
  // unsigned int texture_roughness = renderManager->loadTexture(
  //     "texture_roughness",
  //     fs::path("assets/cerberus/Textures/roughness.png").c_str());

  // unsigned int smoke_texture =
  //     renderManager->loadTexture("smoke", fs::path("assets/smoke.jpg").c_str());

  // unsigned int wood_texture = renderManager->loadTexture(
  //     "wood", fs::path("assets/wooden_texture.png").c_str());

  // unsigned int water_normal_texture = renderManager->loadTexture(
  //     "water_normal", fs::path("assets/water_normal.png").c_str());

  // unsigned int foam_texture =
  //     renderManager->loadTexture("foam", fs::path("assets/foam.png").c_str());

  unsigned int groundTexture = renderManager->loadTexture(
      "groundTexture", fs::path("assets/GroundTexture.png").c_str());

  // unsigned int grassMaskTexture = renderManager->loadTexture(
  //     "grassMaskTexture", fs::path("assets/GrassMask.png").c_str());

  // unsigned int windDistortionTexture = renderManager->loadTexture(
  //     "windDistortionTexture",
  //     fs::path("assets/CircleDisplacementObject.png").c_str());

  // configure global opengl state
  // -----------------------------
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

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

  // build and compile shaders
  // -------------------------
  // Shader shader("3.2.blending.vs", "3.2.blending.fs");

  // set up vertex data (and buffer(s)) and configure vertex attributes
  // ------------------------------------------------------------------

  // cubemap
  vector<std::string> faces = {
      "assets/blue.png", "assets/blue.png",
      "assets/blue.png", "assets/blue.png",
      "assets/blue.png", "assets/blue.png"};
  unsigned int cubemapTexture = loadCubemap(faces);

  std::vector<glm::vec3> objectPositions;
  objectPositions.push_back(glm::vec3(-3.0, -0.5, -3.0));
  objectPositions.push_back(glm::vec3(3.0, -0.5, 3.0));
  objectPositions.push_back(glm::vec3(-0.5, -0.5, 0.0));
  objectPositions.push_back(glm::vec3(8.0, 0.5, 3.0));

  unsigned int sceneFramebuffer;
  unsigned int sceneColorTexture;
  unsigned int sceneDepthTexture;
  glGenFramebuffers(1, &sceneFramebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);

  // Create color texture
  glGenTextures(1, &sceneColorTexture);
  glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, game->SCR_WIDTH, game->SCR_HEIGHT,
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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, game->SCR_WIDTH,
               game->SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
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
    float rColor = static_cast<float>(((rand() % 100) / 200.0f) +
                                      0.5); // between 0.5 and 1.0
    float gColor = static_cast<float>(((rand() % 100) / 200.0f) +
                                      0.5); // between 0.5 and 1.0
    float bColor = static_cast<float>(((rand() % 100) / 200.0f) +
                                      0.5); // between 0.5 and 1.0
    lightColors.push_back(glm::vec3(rColor, gColor, bColor));
  }

  // cubemaps VAO
  unsigned int skyboxVAO, skyboxVBO;
  glGenVertexArrays(1, &skyboxVAO);
  glGenBuffers(1, &skyboxVBO);
  glBindVertexArray(skyboxVAO);
  glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices,
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

  unsigned int framebuffer;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  // shadowmaps

  const unsigned int SHADOW_WIDTH = 1440, SHADOW_HEIGHT = 1440;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  Shader skyboxShader = *renderManager->getShader("skybox_shader");
  Shader shaderLightingPass = *renderManager->getShader("lighting_pass_shader");
  Shader waterShader = *renderManager->getShader("water_shader");
  Shader gridShader2 = *renderManager->getShader("grid_shader_2");
  Shader simpleShader = *renderManager->getShader("simple_shader");
  Shader simpleColorShader = *renderManager->getShader("simple_color_shader");
  Shader grassShader = *renderManager->getShader("grass_shader");
  Shader shaderLightBox = *renderManager->getShader("light_box_shader");
  Shader shaderGeometryPass = *renderManager->getShader("geometry_pass_shader");
  Shader depthPrePass = *renderManager->getShader("depth_pre_pass");

  // shaderLightingPass.use();
  // shaderLightingPass.setInt("gPosition", 0);
  // shaderLightingPass.setInt("gNormal", 1);
  // shaderLightingPass.setInt("gAlbedoSpec", 2);
  // shaderLightingPass.setInt("gDepth", 3);
  // shaderLightingPass.setInt("gLinearDepth", 4);
  // shaderLightingPass.setInt("gMetallic", 5);
  // shaderLightingPass.setInt("gRoughness", 6);
  // shaderLightingPass.setInt("depthMap", 7);

  skyboxShader.use();
  skyboxShader.setInt("skybox", 5);

  // render loop
  // -----------
  Ray ray;
  glm::mat4 viewCopy;
  bool viewFrozen = false;
  bool intersect = false;

  // audioManager->playSource();
  bool selected = false;

  std::vector<glm::vec3> vec = makeThousandVecs();

  lastFrame = static_cast<float>(glfwGetTime());
  renderManager->setRes(game->SCR_WIDTH, game->SCR_HEIGHT);
  renderManager->initializeDepthFBO();
  glm::vec3 lightPos(-1.0f, 4.0f, 1.0f);
  while (!glfwWindowShouldClose(game->getWindow()))
  {

    glm::vec3 lastFrameCameraPos = game->camera.Position;
    float near_plane = 1.10f;
    float far_plane = 1000.0f;
    glm::mat4 model;
    glm::mat4 projection =
        glm::perspective(glm::radians(game->camera.Zoom),
                         (float)game->SCR_WIDTH / (float)game->SCR_HEIGHT,
                         near_plane, far_plane);
    glm::mat4 view = game->camera.GetViewMatrix();

    renderManager->setViewMatrix(view);
    renderManager->setProjectionMatrix(projection);
    // audioManager.loopAudio();
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    ImGuiIO &io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    game->getScene()->handleInput(view, projection, *renderManager);
    game->getScene()->renderGizmo(view, projection);
    ui->screenHeight = game->SCR_HEIGHT;
    ui->screenWidth = game->SCR_WIDTH;
    // Render a white box
    // ui.setProjectionMatrix(projection); // For 1920x1080

    // NOW this is safe:
    ImGui::Text("Camera position %f   %f   %f", game->camera.Position.x,
                game->camera.Position.y, game->camera.Position.z);
    ImGui::Text("Resolution %d   %d", game->SCR_HEIGHT, game->SCR_WIDTH);
    levelEditor->renderImGuiEditor();

    if (shadows)
    {
      // std::cout << "msaa enabled" << std::endl;
      glEnable(GL_MULTISAMPLE);
    }
    else
    {
      // std::cout << "msaa disabled" <<std::endl;
      glDisable(GL_MULTISAMPLE);
    }

    // per-frame time logic
    // --------------------
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    ImGui::Text("Frametime %f", deltaTime);
    ImGui::Text("FPS %f", 1 / deltaTime);

    // std::cout << deltaTime << std::endl;
    lastFrame = currentFrame;

    lightPos.z = static_cast<float>(sin(glfwGetTime() * 1.5) * 3.0);
    // input
    // -----
    game->processGameInput(game->getWindow(), &game->camera, deltaTime, shadows,
                           game->seed);

    if (game->getGameMode() != PAUSE)
    {

      // 1. render depth of scene to texture (from light's perspective)
      // --------------------------------------------------------------
      glm::mat4 lightProjection, lightView;
      glm::mat4 lightSpaceMatrix;
      near_plane = 1.0f, far_plane = 75.5f;
      // lightProjection = glm::perspective(glm::radians(45.0f), (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // note that if you use a perspective projection matrix you'll have to change the light position as the current light position isn't enough to reflect the whole scene
      lightProjection = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, near_plane, far_plane);
      lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
      lightSpaceMatrix = lightProjection * lightView;
      // render scene from light's point of view
      depthPrePass.use();
      depthPrePass.setMat4("lightSpaceMatrix", lightSpaceMatrix);

      glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
      glBindFramebuffer(GL_FRAMEBUFFER, renderManager->depthFBO);
      glClear(GL_DEPTH_BUFFER_BIT);
      glEnable(GL_DEPTH_TEST);
      auto gameObjects = game->getScene()->getGameObjects();
      for (auto &i : gameObjects)
      {
        renderManager->renderGameObjectWithShader(*i, depthPrePass, lightProjection, lightView, i->getModelMatrix());
      }
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      // render
      // ------
      // glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // glViewport(0, 0, game->SCR_WIDTH, game->SCR_HEIGHT);
      // glBindFramebuffer(GL_FRAMEBUFFER, renderManager->depthFBO);
      // glEnable(GL_DEPTH_TEST);
      // glDepthFunc(GL_LESS);
      // glClear(GL_DEPTH_BUFFER_BIT);
      // glDrawBuffer(GL_NONE);
      // auto gameObjects = game->getScene()->getGameObjects();
      // for (auto &i : gameObjects)
      // {
      //   if (i->shaderName == "water_noG")
      //   {
      //     continue;
      //   }
      //   renderManager->renderGameObjectWithShader(*i, depthPrePass);
      // }

      // glBindFramebuffer(GL_FRAMEBUFFER, 0);

      // 1. geometry pass: render scene's geometry/color data into gbuffer
      glViewport(0, 0, game->SCR_WIDTH, game->SCR_HEIGHT);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // ///////////
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      // glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

      // glDisable(GL_DEPTH_TEST); // Disable depth testing for post-processing
      glEnable(GL_DEPTH_TEST); // Re-enable depth testing

      ///
      gameObjects = game->getScene()->getGameObjects();
      for (auto &i : gameObjects)
      {
        if (i->name == "plane_01")
        {
          renderManager->renderGameObjectWithTexture(*i, simpleShader,
                                                     groundTexture);
          continue;
        }
        renderManager->renderGameObject(*i, lightPos, lightSpaceMatrix);
      }

      renderManager->renderSceneToIDBuffer(gameObjects);
      game->getScene()->renderCompactColorPicker();
      int seg = 1000;
      renderManager->renderParabolicTrajectory(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(-2.5f, 2.5f, 0.66f), seg);
      renderManager->renderArrow(glm::vec3(3, 1, 1));
      renderManager->renderEl(glm::vec3(1, 1, 1));
      renderManager->renderLine(glm::vec3(2, 1, 1));

      // renderManager->renderGrass(glm::vec3(0.0f, 1.0f, 0.0f), 0.6, 10, 0.6);

      ////////////////////////////////////////////////////

      // Engine Grid

      // gridShader.use();
      // // model = glm::mat4(1.0f);
      // renderManager->renderGrid(view, projection);

      if (glfwGetKey(game->getWindow(), GLFW_KEY_F) == GLFW_PRESS &&
          selected == false)
      {
        std::cout << "building" << std::endl;
        game->getScene()->addCubeOnTop("simple_color_shader");
        selected = true;
      }
      if (glfwGetKey(game->getWindow(), GLFW_KEY_U) == GLFW_PRESS)
      {
        selected = false;
      }

      if (glfwGetKey(game->getWindow(), GLFW_KEY_C) == GLFW_PRESS &&
          selected == false)
      {
        std::cout << "building" << std::endl;
        game->getScene()->copyEntity();
        selected = true;
      }

      if (game->getGameMode() == ENGINE)
      {
        gridShader2.use();
        gridShader2.setMat4("projection", projection);
        gridShader2.setMat4("view", view);
        model = glm::mat4(1.0f);
        gridShader2.setMat4("model", model);
        renderManager->renderInfiniteGrid(view, game->camera.Position,
                                          gridShader2, 1.0f, 500, 0.02f, 1000);
      }

      glDepthFunc(GL_LEQUAL); // change depth function so depth test passes when
                              // values are equal to depth buffer's content
      skyboxShader.use();
      view = glm::mat4(glm::mat3(
          game->camera
              .GetViewMatrix())); // remove translation from the view matrix
      skyboxShader.setMat4("view", view);
      skyboxShader.setMat4("projection", projection);
      // skybox cube
      glBindVertexArray(skyboxVAO);
      glActiveTexture(GL_TEXTURE5);
      glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
      glDrawArrays(GL_TRIANGLES, 0, 36);
      glBindVertexArray(0);
      glDepthFunc(GL_LESS);
    }
    else if (game->getGameMode() == PAUSE)
    {
      ///////////

      glDisable(GL_DEPTH_TEST);
      glEnable(GL_BLEND);
      // ui->renderUIBBox(1300.0f, 250.0f, -700.0f, 1100.0f);

      // // For softer blending
      // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      // // Or for additive blending (glowing effect)
      // glBlendFunc(GL_SRC_ALPHA, GL_ONE);

      // // Render with coordinated colors

      // ui->RenderText(std::to_string(game->camera.Position.x), 10.0f, 10.0f, 1.0f,
      // glm::vec3(1.0, 0.0f, 0.0f));
      // ui->RenderText(std::to_string(game->camera.Position.y), 10.0f, 50.0f, 1.0f,
      // glm::vec3(1.0, 0.0f, 0.0f));
      ui->renderPauseMenu();
      glDisable(GL_BLEND);

      ///////////
    }

    ///////////////////////////////////////////////////
    game->update(deltaTime);

    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(game->getWindow());
    glfwPollEvents();
    FrameMark;
  }

  //
  audioManager->cleanUp();

  // optional: de-allocate all resources once they've outlived their purpose:
  // ------------------------------------------------------------------------
  // glDeleteVertexArrays(1, &planeVAO);
  // glDeleteBuffers(1, &planeVBO);
  // glDeleteFramebuffers(1, &fbo);

  glfwTerminate();
  return 0;
}

unsigned int loadCubemap(vector<std::string> faces)
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
