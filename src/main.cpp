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

  // ─── Vulkan setup ────────────────────────────────────────────────────────
  // Try unified window first (Vulkan surface on the main GLFW window via a
  // separate Metal layer on macOS). Fall back to a dedicated second window.
  VulkanContext vulkanContext;
  VulkanRenderer vulkanRenderer;
  bool vulkanReady = false;
  bool useVulkan = false;

  bool vkCtxOk = vulkanContext.initFromExistingWindow(
      game->getWindow(), game->SCR_WIDTH, game->SCR_HEIGHT);
  if (!vkCtxOk) {
      vkCtxOk = vulkanContext.init(game->SCR_WIDTH, game->SCR_HEIGHT);
  }
  if (vkCtxOk && vulkanRenderer.init(&vulkanContext)) {
      vulkanReady = true;
      useVulkan   = true;
      if (vulkanContext.ownsWindow()) {
          // Separate second window: hide OpenGL window, mirror inputs
          vulkanContext.showWindow();
          glfwHideWindow(game->getWindow());
          game->registerInputCallbacksOnWindow(vulkanContext.getWindow());
          game->setInputWindow(vulkanContext.getWindow());
      }
      std::cout << "[Vulkan] Ready — "
                << (vulkanContext.ownsWindow() ? "second window" : "unified window")
                << std::endl;
      vulkanRenderer.loadScene(game->getScene());
  }

  // view is updated each frame; declared here so vulkanEngineCallback (below) can capture it by ref
  glm::mat4 view(1.0f);

  // ─── Vulkan ENGINE callback ───────────────────────────────────────────────
  // Invoked inside the Vulkan ImGui frame so external systems (level editor,
  // camera info, UI elements, pause overlay) appear in the Vulkan window.
  auto vulkanEngineCallback = [&]() {
      int vkFbW = 1, vkFbH = 1;
      glfwGetFramebufferSize(vulkanContext.getWindow(), &vkFbW, &vkFbH);

      if (game->getGameMode() == ENGINE) {
          ImGui::Text("Camera  %.2f  %.2f  %.2f",
              game->camera.Position.x, game->camera.Position.y, game->camera.Position.z);
          ImGui::Text("Resolution  %d x %d", game->SCR_WIDTH, game->SCR_HEIGHT);

          if (ImGui::CollapsingHeader("Trajectory Debug")) {
              bool traj = renderManager->getRenderTrajectory();
              if (ImGui::Checkbox("Render Trajectory", &traj))
                  renderManager->setRenderTrajectory(traj);
              if (traj) {
                  int segs = renderManager->getTrajectorySegments();
                  if (ImGui::SliderInt("Segments", &segs, 10, 1000))
                      renderManager->setTrajectorySegments(segs);
              }
          }

          ImGui::Separator();
          levelEditor->renderImGuiEditor();
      }

      // Vulkan toggle (visible regardless of game mode)
      ImGui::Separator();
      bool prev = useVulkan;
      ImGui::Checkbox("Use Vulkan Renderer", &useVulkan);
      if (useVulkan != prev && !useVulkan && vulkanContext.ownsWindow()) {
          // Switching back to OpenGL (second-window mode only)
          vulkanContext.hideWindow();
          glfwShowWindow(game->getWindow());
          game->setInputWindow(nullptr);
          glfwSetInputMode(game->getWindow(), GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
      }

      // ── UI elements: build, click detection, render ───────────────────────
      // Must call the build functions every frame — they clear + repopulate uiElements.
      uiManager->clearUIElements();
      uiManager->buildGameMenu();
      uiManager->buildActionBufferUI();
      uiManager->buildBottomCenterMenu();

      // Click detection. UIManager stores Y in OpenGL bottom-left space;
      // isMouseOver flips the raw GLFW mouse Y internally.
      {
          double mx, my;
          glfwGetCursorPos(vulkanContext.getWindow(), &mx, &my);
          static bool wasUIPressed = false;
          bool isUIPressed = glfwGetMouseButton(vulkanContext.getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
          bool justClicked = isUIPressed && !wasUIPressed;
          wasUIPressed = isUIPressed;

          if (justClicked && !ImGui::GetIO().WantCaptureMouse) {
              for (auto& elem : uiManager->getUIElements()) {
                  if (elem.elementType == BOX && elem.functionPtr &&
                      uiManager->isMouseOver((float)mx, (float)my,
                          elem.width, elem.height, elem.x_pos, elem.y_pos)) {
                      elem.functionPtr("clicked");
                      break;
                  }
              }
          }
      }

      // Render via ImGui background draw list.
      // UIManager Y is bottom-left (OpenGL). ImGui Y is top-left. Flip: iy = H - y - h.
      auto* dl = ImGui::GetBackgroundDrawList();
      ImFont* font = ImGui::GetFont();
      for (auto& elem : uiManager->getUIElements()) {
          ImU32 col = IM_COL32(
              (int)(elem.r * 255), (int)(elem.g * 255),
              (int)(elem.b * 255), (int)(elem.a * 255));
          if (elem.elementType == BOX) {
              float iy = (float)vkFbH - elem.y_pos - elem.height;
              dl->AddRectFilled(
                  ImVec2(elem.x_pos, iy),
                  ImVec2(elem.x_pos + elem.width, iy + elem.height),
                  col);
          } else if (elem.elementType == TEXT && !elem.text.empty()) {
              float fontSize = std::max(8.0f, elem.scale * 20.0f);
              float iy = (float)vkFbH - elem.y_pos - fontSize;
              dl->AddText(font, fontSize,
                  ImVec2(elem.x_pos, iy), col, elem.text.c_str());
          }
      }

      // ── Parabolic trajectory (world → screen projection) ──────────────────
      if (game->getScene()->ball && renderManager->getRenderTrajectory()) {
          glm::vec3 start  = game->getScene()->ball->position;
          glm::vec3 target = start + glm::vec3(-2.5f, 0.0f, 0.66f); // same offset as GL path
          float     peak   = 2.5f;
          int       segs   = renderManager->getTrajectorySegments();

          float aspect = (vkFbH > 0) ? (float)vkFbW / (float)vkFbH : 1.0f;
          glm::mat4 vkProj = glm::perspective(
              glm::radians(game->camera.Zoom), aspect, 1.1f, 1000.0f);

          auto worldToScreen = [&](glm::vec3 wp) -> ImVec2 {
              glm::vec4 clip = vkProj * view * glm::vec4(wp, 1.0f);
              if (clip.w <= 0.0f) return ImVec2(-9999, -9999);
              glm::vec3 ndc = glm::vec3(clip) / clip.w;
              return ImVec2(
                  (ndc.x * 0.5f + 0.5f) * vkFbW,
                  (1.0f - (ndc.y * 0.5f + 0.5f)) * vkFbH);
          };

          ImU32 trajCol = IM_COL32(230, 200, 60, 220);
          for (int i = 0; i < segs; ++i) {
              float t0 = (float)i       / segs;
              float t1 = (float)(i + 1) / segs;
              glm::vec3 p0 = glm::mix(start, target, t0);
              glm::vec3 p1 = glm::mix(start, target, t1);
              p0.y += peak * 4.0f * t0 * (1.0f - t0);
              p1.y += peak * 4.0f * t1 * (1.0f - t1);
              dl->AddLine(worldToScreen(p0), worldToScreen(p1), trajCol, 2.0f);
          }
      }

      // ── Pause menu ────────────────────────────────────────────────────────
      if (game->getGameMode() == PAUSE) {
          float sw = (float)vkFbW, sh = (float)vkFbH;
          float panelW = sw * 0.4f, panelH = sh * 0.6f;
          float panelX = (sw - panelW) * 0.5f, panelY = (sh - panelH) * 0.5f;
          float scale  = sh / 1440.0f;

          // Full-screen dim overlay
          dl->AddRectFilled(ImVec2(0, 0), ImVec2(sw, sh),
              IM_COL32(25, 25, 38, 242));
          // Panel background
          dl->AddRectFilled(ImVec2(panelX, panelY),
              ImVec2(panelX + panelW, panelY + panelH),
              IM_COL32(51, 51, 64, 255));
          // Gold top accent
          dl->AddRectFilled(ImVec2(panelX, panelY),
              ImVec2(panelX + panelW, panelY + 3.0f),
              IM_COL32(230, 204, 77, 255));

          float fs    = std::max(8.0f, 24.0f * scale);
          float titleFs = fs * 1.5f;
          ImU32 gold  = IM_COL32(230, 204, 77, 255);
          ImU32 white = IM_COL32(255, 255, 255, 255);

          // Title
          float titleX = panelX + panelW * 0.35f;
          float titleY = panelY + 20.0f * scale;
          dl->AddText(font, titleFs, ImVec2(titleX, titleY), gold, "PAUSED");

          // Menu items
          float itemY    = panelY + panelH * 0.35f;
          float itemStep = 60.0f * scale;
          float itemX    = panelX + panelW * 0.4f;
          dl->AddText(font, fs, ImVec2(itemX, itemY),                white, "Resume");
          dl->AddText(font, fs, ImVec2(itemX, itemY + itemStep),     white, "Settings");
          dl->AddText(font, fs, ImVec2(itemX, itemY + itemStep * 2), white, "Exit");
      }
  };

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
  while (!glfwWindowShouldClose(game->getWindow()) &&
         !(useVulkan && vulkanReady && glfwWindowShouldClose(vulkanContext.getWindow())))
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
    view = game->camera.GetViewMatrix();

    renderManager->setViewMatrix(view);
    renderManager->setProjectionMatrix(projection);
    // audioManager.loopAudio();
    // Start ImGui frame (OpenGL context — Vulkan has its own in drawFrame)
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    ImGuiIO &io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    if (!useVulkan) {
      // Scene input (OpenGL object-ID picking + game logic) and the OpenGL
      // gizmo both depend on the OpenGL ImGui context and renderManager's
      // ID buffer — skip them when Vulkan is active.
      game->getScene()->handleInput(view, projection, *renderManager);
    }
    if (game->getGameMode() == ENGINE)
    {
      if (!useVulkan) {
        game->getScene()->renderGizmo(view, projection);
      }

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
        if (useVulkan != prev && vulkanContext.ownsWindow()) {
          if (useVulkan) {
            vulkanContext.showWindow();
            glfwHideWindow(game->getWindow());
            game->registerInputCallbacksOnWindow(vulkanContext.getWindow());
            game->setInputWindow(vulkanContext.getWindow());
          } else {
            vulkanContext.hideWindow();
            glfwShowWindow(game->getWindow());
            game->setInputWindow(nullptr);
            glfwSetInputMode(game->getWindow(), GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
          }
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
      {
        // Feed camera matrices to Vulkan renderer, with correct aspect ratio
        vulkanRenderer.setViewMatrix(view);
        int vkW, vkH;
        glfwGetFramebufferSize(vulkanContext.getWindow(), &vkW, &vkH);
        if (vkW > 0 && vkH > 0) {
          glm::mat4 vkProjection = glm::perspective(
              glm::radians(game->camera.Zoom),
              static_cast<float>(vkW) / static_cast<float>(vkH),
              near_plane, far_plane);
          vulkanRenderer.setProjectionMatrix(vkProjection);
        } else {
          vulkanRenderer.setProjectionMatrix(projection);
        }
        if (!vulkanRenderer.drawFrame(vulkanEngineCallback)) {
          int w, h;
          glfwGetFramebufferSize(vulkanContext.getWindow(), &w, &h);
          if (w > 0 && h > 0) {
            vulkanRenderer.handleResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
          }
        }

        // Mouse picking on Vulkan window (left click, single-shot).
        // Skip when ImGui is capturing the mouse (e.g. user clicked a panel).
        {
          static bool wasPressed = false;
          bool isPressed = glfwGetMouseButton(vulkanContext.getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

          // Read WantCaptureMouse from the Vulkan ImGui context
          bool imguiWantsMouse = false;
          if (ImGuiContext* vkCtx = vulkanRenderer.getImGuiContext()) {
              ImGuiContext* prevCtx = ImGui::GetCurrentContext();
              ImGui::SetCurrentContext(vkCtx);
              imguiWantsMouse = ImGui::GetIO().WantCaptureMouse;
              ImGui::SetCurrentContext(prevCtx);
          }

          if (isPressed && !wasPressed && !imguiWantsMouse) {
            double mx, my;
            glfwGetCursorPos(vulkanContext.getWindow(), &mx, &my);

            // Scale screen coords → framebuffer pixels (Retina = 2x)
            int winW, winH;
            glfwGetWindowSize(vulkanContext.getWindow(), &winW, &winH);
            float scaleX = static_cast<float>(vkW) / static_cast<float>(winW);
            float scaleY = static_cast<float>(vkH) / static_cast<float>(winH);
            int px = static_cast<int>(mx * scaleX);
            int py = static_cast<int>(my * scaleY);

            uint32_t pickedID = vulkanRenderer.getObjectIdAtPixel(px, py);
            std::cout << "[Vulkan] Pick at (" << px << "," << py << ") -> ID " << pickedID << std::endl;
          }
          wasPressed = isPressed;
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

    // Render ImGui via OpenGL only when OpenGL is the active renderer,
    // or when Vulkan has its own separate window (hidden GL window is harmless).
    // Skip in unified-window Vulkan mode to avoid drawing over Vulkan's image.
    ImGui::Render();
    if (!useVulkan || vulkanContext.ownsWindow()) {
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

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
