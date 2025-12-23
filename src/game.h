#define GLM_ENABLE_EXPERIMENTAL
#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <../src/shader_m.h>
#include <../src/camera.h>
#include <../src/model.h>

#include <../include/stb_image.h>
#include <../src/input.h>

#include <../src/game_object.h>
#include <../src/scene.h>

#include <iostream>
#include <../json/single_include/nlohmann/json.hpp>
#include <filesystem>

#include <chrono>
#include <thread>

#include <../src/raycast.h>

#include "../src/render_manager.h"

#include "imgui.h"
#include "../src/audio_manager.h"
#include "../src/sphere_collision.h"
#include "game_manager.h"
#include "ui.h"

class InputManager;

enum GameModeEnum
{
    GAME,
    ENGINE,
    PAUSE,
};

class Game
{
private:
    // Core systems
    // EntityManager entityManager;
    RenderManager renderManager;
    // InputManager inputManager;
    AudioManager audioManager;
    // SceneManager sceneManager;
    // PhysicsManager physicsManager;
    // ResourceManager resourceManager;
    SphereCollision sphereCollision;
    InputManager inputManager;
    GameManager gameManager;
    std::shared_ptr<UIManager> uiManager;

    // Game state
    // GameState currentState;
    bool isRunning;
    GameModeEnum mode;

    // Window/context
    GLFWwindow *window;
    std::shared_ptr<Scene> scene;

    glm::vec3 engineCameraPos;
    glm::vec3 engineCameraFront;

    // timing
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    bool MULTISAMPLE = true;

    void handleInput();

public:
    bool initialize();
    void run();
    void update();
    void render();
    void cleanup();

    Game();
    ~Game();

    void setLevel(std::string levelName);

    InputManager *getInputManager() { return &inputManager; };
    GameManager *getGameManager() { return &gameManager; };
    std::shared_ptr<UIManager> getUIManager() { return uiManager; };

    // System accessors
    // EntityManager& getEntityManager() { return entityManager; }
    RenderManager &getRenderManager() { return renderManager; }
    AudioManager &getAudioManager() { return audioManager; }
    GLFWwindow *getWindow() { return window; }

    void setScene(std::shared_ptr<Scene> newScene) { scene = newScene; };
    Scene *getScene() { return scene.get(); };

    void setGameMode(GameModeEnum modeEnum)
    {
        mode = modeEnum;
        if (modeEnum == GAME)
        {
            camera.gameMode = true;
            // engineCameraPos = camera.Position;
            // engineCameraFront = camera.Front;
            // camera.Position = glm::vec3(0, 10, 0);
            // camera.Front = glm::vec3(0, -1, 0);
        }
        if (modeEnum == ENGINE)
        {
            camera.gameMode = false;
            // camera.Position = engineCameraPos;
            // camera.Front = engineCameraFront;
        }
    };
    GameModeEnum getGameMode() { return mode; };

    void processGameInput(GLFWwindow *window, Camera *camera, float deltaTime, bool &shadows, float &seed);

    unsigned int SCR_WIDTH = 1440;
    unsigned int SCR_HEIGHT = 1440;

    bool firstMouse = true;

    float lastX = SCR_WIDTH / 2.0f;
    float lastY = SCR_HEIGHT / 2.0f;

    float seed = rand();

    float xpos = 0;
    float ypos = 0;

    Camera camera;
    // InputManager& getInputManager() { return inputManager; }
    // SceneManager& getSceneManager() { return sceneManager; }
    // ... other getters

    bool initWindow()
    {

        camera = Camera(glm::vec3(0.0f, 10.0f, 3.0f));
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 8);           // Request 8x MSAA
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // Make window resizable

#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        // glfw window creation
        // --------------------
        window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Dreaming...", NULL, NULL);
        if (window == NULL)
        {
            std::cout << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }
        glfwMakeContextCurrent(window);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);

        // tell GLFW to capture our mouse
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);

        // glad: load all OpenGL function pointers
        // ---------------------------------------
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cout << "Failed to initialize GLAD" << std::endl;
            return -1;
        }
        glfwSwapInterval(0); // 0 = disable V-Sync, 1 = enable
                             // TODO: Initialize GLFW, create window, setup OpenGL context
                             // TODO: Initialize all managers (render, input, audio, etc.)
                             // TODO: Load initial resources
                             // TODO: Setup initial game state

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        return true;
    }

    static void framebuffer_size_callback(GLFWwindow *window, int width, int height)
    {
        Game *game = static_cast<Game *>(glfwGetWindowUserPointer(window));
        if (game)
            game->framebuffer_size_callback_impl(width, height);
    }

    static void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
    {
        Game *game = static_cast<Game *>(glfwGetWindowUserPointer(window));
        if (game)
            game->mouse_callback_impl(xposIn, yposIn);
    }

    static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
    {
        Game *game = static_cast<Game *>(glfwGetWindowUserPointer(window));
        if (game)
            game->scroll_callback_impl(xoffset, yoffset);
    }

    void framebuffer_size_callback_impl(int width, int height)
    {
        // Update the viewport
        glViewport(0, 0, width, height);

        // Update screen dimensions
        SCR_WIDTH = width;
        SCR_HEIGHT = height;

        // Update camera aspect ratio if your camera supports it
        if (width > 0 && height > 0)
        {
            // camera.SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
        }

        // Notify render manager about the resize if it needs to update framebuffers
        renderManager.setRes(width, height);
        renderManager.onWindowResize(width, height);
    }

    void mouse_callback_impl(double xposIn, double yposIn)
    {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;

        camera.ProcessMouseMovement(xoffset, yoffset);
    }

    void scroll_callback_impl(double xoffset, double yoffset)
    {
        camera.ProcessMouseScroll(static_cast<float>(yoffset));
    }

    // Helper method to get current window dimensions
    void getCurrentWindowSize(int &width, int &height)
    {
        glfwGetWindowSize(window, &width, &height);
        SCR_WIDTH = width;
        SCR_HEIGHT = height;
    }
};