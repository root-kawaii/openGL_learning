// OpenGL headers - MUST be first
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// OpenAL headers
#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
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
#include "vulkan/vk_context.h"
#include "vulkan/vk_renderer.h"

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

  // ─── Vulkan setup (runs alongside OpenGL) ───────────────────────────────
  VulkanContext vulkanContext;
  VulkanRenderer vulkanRenderer;
  bool vulkanReady = false;
  bool useVulkan = false; // Toggle with ImGui checkbox

  if (vulkanContext.init(game->SCR_WIDTH, game->SCR_HEIGHT)) {
    if (vulkanRenderer.init(&vulkanContext)) {
      vulkanReady = true;
      std::cout << "[Vulkan] Ready — toggle with checkbox in ENGINE mode" << std::endl;
    }
  }

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

      ImGui::Separator();
      if (vulkanReady) {
        bool prev = useVulkan;
        ImGui::Checkbox("Use Vulkan Renderer", &useVulkan);
        if (useVulkan != prev) {
          if (useVulkan) vulkanContext.showWindow();
          else           vulkanContext.hideWindow();
        }
      } else {
        ImGui::TextDisabled("Vulkan not available");
      }

      levelEditor->renderImGuiEditor();
    }
    verticesDrawn = 0;
    trianglesDrawn = 0;
    drawCalls = 0;

    if (useVulkan)
    {
      // ─── Vulkan rendering path ─────────────────────────────────────────
      // For now, just clear the screen. ImGui still renders via OpenGL on top.
      // Check if user closed the Vulkan window
      if (glfwWindowShouldClose(vulkanContext.getWindow())) {
        useVulkan = false;
        vulkanContext.hideWindow();
        glfwSetWindowShouldClose(vulkanContext.getWindow(), GLFW_FALSE);
      } else {
        if (!vulkanRenderer.drawFrame()) {
          int w, h;
          glfwGetFramebufferSize(vulkanContext.getWindow(), &w, &h);
          if (w > 0 && h > 0) {
            vulkanRenderer.handleResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
          }
        }
      }

      // Still clear the OpenGL window so ImGui draws cleanly
      glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    else
    {
      // ─── OpenGL rendering path (original) ──────────────────────────────
      glEnable(GL_MULTISAMPLE);

      if (game->getGameMode() != PAUSE)
      {
        renderManager->renderShadowPass();
        Model::BeginFrame();
        renderManager->prepareAllAnimations();
        glViewport(0, 0, game->SCR_WIDTH, game->SCR_HEIGHT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        renderManager->renderMainPass();
        renderManager->renderRainPass(deltaTime);

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

        uiManager->renderAllUIElements(game->lastX, game->lastY);
        if (game->getScene()->ball)
          renderManager->renderParabolicTrajectory(game->getScene()->ball->position, glm::vec3(5, 5, 5), 200);
      }
      else if (game->getGameMode() == PAUSE)
      {
        uiManager->renderPauseMenu();
      }

      if (game->getGameMode() == VISUAL_NOVEL)
      {
        game->getVNManager().render(game->SCR_WIDTH, game->SCR_HEIGHT);
      }
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

    // Render ImGui (always via OpenGL for now)
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Only swap OpenGL backbuffer when OpenGL is rendering.
    // When Vulkan is active, it presents via its own swapchain.
    if (!useVulkan) {
      glfwSwapBuffers(game->getWindow());
    }
    glfwPollEvents();
    FrameMark;
  }

  // Vulkan cleanup (before GLFW terminates the window)
  vulkanRenderer.cleanup();
  vulkanContext.cleanup();

  audioManager->cleanUp();

  // optional: de-allocate all resources once they've outlived their purpose:
  // ------------------------------------------------------------------------
  // glDeleteVertexArrays(1, &planeVAO);
  // glDeleteBuffers(1, &planeVBO);
  // glDeleteFramebuffers(1, &fbo);

  glfwTerminate();
  return 0;
}
