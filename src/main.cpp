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

#include "globals.h"

// Note: Uncomment these if needed
// #include <../src/scene.h>
// #include "../src/texture_debugger.cpp"

#include <random>
#include <vector>

namespace fs = std::filesystem;

// meshes
unsigned int planeVAO;

// fired
bool fired = false;

int main()
{

  std::shared_ptr<Game> game = std::make_shared<Game>();

  // Get pointers to managers (assuming they're already heap-allocated in Game)
  std::shared_ptr<UIManager> uiManager = game->getUIManager();
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

  renderManager->setRes(game->SCR_WIDTH, game->SCR_HEIGHT);
  renderManager->setScene(game->getScene());
  renderManager->initializeDepthFBO();

  // configure global opengl state
  // -----------------------------
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  unsigned int framebuffer;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // shadowmaps

  const unsigned int SHADOW_WIDTH = 1440, SHADOW_HEIGHT = 1440;

  // Shader skyboxShader = *renderManager->getShader("skybox_shader");
  Shader gridShader2 = *renderManager->getShader("grid_shader_2");
  Shader depthPrePass = *renderManager->getShader("depth_pre_pass");

  // audioManager->playSource();
  bool selected = false;

  glm::vec3 lightPos(-1.0f, 4.0f, 1.0f);

  float deltaTime = 0.0f;
  float lastFrame = 0.0f;
  while (!glfwWindowShouldClose(game->getWindow()))
  {
    uiManager->screenHeight = game->SCR_HEIGHT;
    uiManager->screenWidth = game->SCR_WIDTH;
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
    if (game->getGameMode() == ENGINE)
    {
      game->getScene()->renderGizmo(view, projection);

      ImGui::Text("Camera position %f   %f   %f", game->camera.Position.x,
                  game->camera.Position.y, game->camera.Position.z);
      ImGui::Text("Resolution %d   %d", game->SCR_HEIGHT, game->SCR_WIDTH);
      ImGui::Text("Vertices drawn %d", verticesDrawn);
      ImGui::Text("Triangles drawn %d", trianglesDrawn);
      ImGui::Text("Draw calls %d ", drawCalls);

      // Trajectory rendering controls
      if (ImGui::CollapsingHeader("Trajectory Debug"))
      {
        bool enabled = renderManager->getRenderTrajectory();
        if (ImGui::Checkbox("Render Trajectory", &enabled))
        {
          renderManager->setRenderTrajectory(enabled);
        }

        if (enabled)
        {
          int segments = renderManager->getTrajectorySegments();
          if (ImGui::SliderInt("Segments", &segments, 10, 1000))
          {
            renderManager->setTrajectorySegments(segments);
          }
        }
      }

      levelEditor->renderImGuiEditor();
    }
    verticesDrawn = 0;
    trianglesDrawn = 0;
    drawCalls = 0;

    if (true)
    {
      // std::cout << "msaa enabled" << std::endl;
      glEnable(GL_MULTISAMPLE);
    }
    else
    {
      // std::cout << "msaa disabled" <<std::endl;
      glDisable(GL_MULTISAMPLE);
    }

    if (game->getGameMode() != PAUSE)
    {

      renderManager->renderShadowPass();
      // 1. geometry pass: render scene's geometry/color data into gbuffer
      glViewport(0, 0, game->SCR_WIDTH, game->SCR_HEIGHT);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glEnable(GL_DEPTH_TEST); // Re-enable depth testing

      renderManager->renderMainPass(); // work on this

      renderManager->renderRainPass(deltaTime);

      // ///////////
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
      else
      {
        // Render arrow above selected game entity in GAME mode
        auto selected = game->getSelectedEntity();
        // if (selected && selected->object)
        // {
        //   renderManager->renderVerticalArrow(selected->object->position + glm::vec3(0, 4.15, 0));
        // }
      }

      uiManager->renderAllUIElements(game->lastX, game->lastY); // 200 microseconds ????? seems ok actually
      if (game->getScene()->ball)
        renderManager->renderParabolicTrajectory(game->getScene()->ball->position, glm::vec3(5, 5, 5), 200);
    }
    else if (game->getGameMode() == PAUSE)
    {

      uiManager->renderPauseMenu();
    }

    // VN overlay — renders on top of live 3D scene
    if (game->getGameMode() == VISUAL_NOVEL)
    {
      game->getVNManager().render(game->SCR_WIDTH, game->SCR_HEIGHT);
    }

    ///////////////////////////////////////////////////
    game->update();

    // Shader hot-reload: update timer and check for shader changes (ENGINE mode only)
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    game->getVNManager().update(deltaTime);

    if (game->getGameMode() == ENGINE)
    {
      renderManager->setTimeSinceLastShaderReload(renderManager->getTimeSinceLastShaderReload() + deltaTime);
      renderManager->checkAndReloadShaders();
    }

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
