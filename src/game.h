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
#include <../src/vn_manager.h>

#include <iostream>
#include <../json/single_include/nlohmann/json.hpp>
#include <filesystem>
#include <set>
#include <vector>
#include <string>
#include <memory>

#include <chrono>
#include <thread>
#include <optional>

#include <../src/raycast.h>

#include "../src/render_manager.h"

#include "imgui.h"
#include "../src/audio_manager.h"
#include "../src/sphere_collision.h"
#include "../src/collision_system.h"
#include "../src/projectile_bounce_utils.h"
#include "game_manager.h"
#include "ui.h"
#include "player.h"

class InputManager;

// Settings structure for game configuration
struct GameSettings
{
    // Graphics
    unsigned int resolutionWidth = 1920;
    unsigned int resolutionHeight = 1080;
    bool fullscreen = false;

    // Audio
    float masterVolume = 0.8f;

    // Gameplay
    std::string difficulty = "normal";
    bool autosave = true;
    int autosaveInterval = 300;

    // Controls
    bool invertY = false;
};

enum GameModeEnum
{
    GAME,
    ENGINE,
    PAUSE,
    VISUAL_NOVEL,
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
    SphereCollision sphereCollision; // Legacy - TODO: Remove after migration
    CollisionSystem collisionSystem; // New optimized collision system
    InputManager inputManager;
    GameManager gameManager;
    std::shared_ptr<UIManager> uiManager;
    VNManager vnManager;

    // Game state
    // GameState currentState;
    bool isRunning;
    GameModeEnum mode;

    // Window/context
    GLFWwindow *window;
    GLFWwindow *inputWindow = nullptr;  // Active input window (Vulkan window when in Vulkan mode)
    std::shared_ptr<Scene> scene;

    glm::vec3 engineCameraPos;
    glm::vec3 engineCameraFront;

    // timing
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    bool MULTISAMPLE = true;

    bool  firstPersonEnabled = false;
    bool  firstPersonGrounded = false;
    bool  firstPersonJumpPressedLastFrame = false;
    float firstPersonVerticalVelocity = 0.0f;
    float firstPersonEyeHeight = 2.0f;
    float firstPersonRadius = 0.35f;
    float firstPersonHeight = 2.0f;
    float firstPersonJumpVelocity = 5.75f;
    float firstPersonGravity = 18.0f;
    float firstPersonMoveSpeed = 5.5f;
    float firstPersonStepHeight = 0.6f;
    float firstPersonGroundSnap = 0.15f;
    float firstPersonAimBlend = 0.0f;
    float firstPersonShootCooldown = 0.0f;
    float firstPersonShootAnimTime = 0.0f;
    float firstPersonWeaponBobTime = 0.0f;
    std::shared_ptr<GameObject> firstPersonWeaponObject;
    std::shared_ptr<Model> firstPersonProjectileModel;
    std::shared_ptr<AudioClip> firstPersonShotClip;
    std::shared_ptr<AudioClip> torchAmbientClip;
    uint32_t firstPersonProjectileCounter = 0;
    glm::vec3 previousAudioListenerPosition = glm::vec3(0.0f);
    bool audioListenerPrimed = false;

    struct TorchAudioEmitter
    {
        std::weak_ptr<GameObject> object;
        std::shared_ptr<AudioSource3D> source;
    };
    std::vector<TorchAudioEmitter> torchAudioEmitters;

    struct FirstPersonProjectile
    {
        std::shared_ptr<GameObject> object;
        glm::vec3 velocity = glm::vec3(0.0f);
        float lifetime = 0.0f;
        bool impacted = false;
        int remainingBounces = 0;
    };
    std::vector<FirstPersonProjectile> firstPersonProjectiles;

    void handleInput();
    void loadSettings();
    void enterFirstPersonGameMode();
    void updateFirstPersonController();
    void createFirstPersonWeapon();
    void spawnFirstPersonProjectile(const glm::vec3 &origin, const glm::vec3 &velocity);
    void updateFirstPersonProjectiles();
    void clearFirstPersonProjectiles();
    void updateFirstPersonWeapon();
    void hideFirstPersonWeapon();
    void rebuildTorchAudioEmitters();
    void updateTorchAudioEmitters();
    void clearTorchAudioEmitters();
    bool shouldAttachTorchAudio(const std::shared_ptr<GameObject> &obj) const;
    glm::vec3 resolveFirstPersonCollisions(const glm::vec3 &targetCameraPos,
                                           const glm::vec3 &currentCameraPos,
                                           bool &grounded,
                                           float &groundHeight) const;
    std::optional<glm::vec3> findGameplaySpawnPoint() const;
    bool isFirstPersonSolid(const std::shared_ptr<GameObject> &obj) const;

    int turn = 0;
    // int turnClock = 0; // from 0 to 24

    void handleTurn();

    std::vector<Player> players;

    // Turn management (player-based, not entity-based)
    std::shared_ptr<GameEntity> selectedEntity; // Currently selected entity
    std::set<std::string> selectableEntityTags = {"88", "unit", "character"};

    // Collision detection
    void detectCollisions(const std::vector<std::pair<std::shared_ptr<GameEntity>, glm::vec3>> &tickMovements);
    void handleSameCellCollision(glm::ivec3 cell, const std::vector<std::shared_ptr<GameEntity>> &entities);
    void handleCrossingCollision(std::shared_ptr<GameEntity> entityA, std::shared_ptr<GameEntity> entityB);

public:
    // Turn-based movement system
    enum class TurnState
    {
        PLANNING,  // Players queue movements
        EXECUTING, // Movements being executed
        RESOLVING  // Animations playing
    };

    TurnState turnState = TurnState::PLANNING;
    bool allMovementsComplete = true;
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

    // Register mouse/scroll callbacks on any GLFW window (used to mirror input to Vulkan window)
    void registerInputCallbacksOnWindow(GLFWwindow* w) {
        glfwSetWindowUserPointer(w, this);
        glfwSetCursorPosCallback(w, mouse_callback);
        glfwSetScrollCallback(w, scroll_callback);
        glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
    }

    // Switch which window keyboard polling and cursor reads come from
    void setInputWindow(GLFWwindow* w) {
        inputWindow = w;
        firstMouse = true;  // Reset mouse delta on window switch to avoid jump
    }

    // System accessors
    // EntityManager& getEntityManager() { return entityManager; }
    RenderManager &getRenderManager() { return renderManager; }
    AudioManager &getAudioManager() { return audioManager; }
    GLFWwindow *getWindow() { return window; }

    void setScene(std::shared_ptr<Scene> newScene) { scene = newScene; };
    Scene *getScene() { return scene.get(); };

    void setGameMode(GameModeEnum modeEnum)
    {
        GameModeEnum previousMode = mode;
        mode = modeEnum;
        if (modeEnum == GAME)
        {
            camera.gameMode = true;
            if (previousMode == PAUSE)
            {
                firstPersonEnabled = true;
                GLFWwindow *activeWindow = inputWindow ? inputWindow : window;
                if (activeWindow)
                    glfwSetInputMode(activeWindow, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
            }
            else
            {
                enterFirstPersonGameMode();
            }
        }
        if (modeEnum == ENGINE)
        {
            camera.gameMode = false;
            firstPersonEnabled = false;
            firstPersonGrounded = false;
            firstPersonJumpPressedLastFrame = false;
            firstPersonVerticalVelocity = 0.0f;
            firstPersonAimBlend = 0.0f;
            firstPersonShootCooldown = 0.0f;
            firstPersonShootAnimTime = 0.0f;
            hideFirstPersonWeapon();
            clearFirstPersonProjectiles();
        }
        if (modeEnum == PAUSE)
        {
            firstPersonVerticalVelocity = 0.0f;
        }
    };
    GameModeEnum getGameMode() { return mode; };
    VNManager&   getVNManager() { return vnManager; }

    // Turn-based gameplay methods
    void endPlayerTurn();
    void resetAllEntityMovement();
    bool isEntitySelectable(std::shared_ptr<GameEntity> entity);
    bool canEntityMove(std::shared_ptr<GameEntity> entity);
    void setSelectedEntity(std::shared_ptr<GameEntity> entity);
    std::shared_ptr<GameEntity> getSelectedEntity() { return selectedEntity; }
    int getTurnNumber() const { return turn; }

    // Queued movement system
    void startTurnExecution();
    void updateTurnExecution();
    bool checkAllMovementsComplete();
    TurnState getTurnState() const { return turnState; }

    void processGameInput(GLFWwindow *window, Camera *camera, float deltaTime, bool &shadows, float &seed, RenderManager *renderManager);

    // Game settings
    GameSettings settings;

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

        // Get primary monitor and its video mode to detect resolution
        GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(primaryMonitor);

        // Use monitor resolution for window size
        SCR_WIDTH = mode->width;
        SCR_HEIGHT = mode->height;

        std::cout << "Detected monitor resolution: " << SCR_WIDTH << "x" << SCR_HEIGHT << std::endl;

        // Update mouse position defaults
        lastX = SCR_WIDTH / 2.0f;
        lastY = SCR_HEIGHT / 2.0f;

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
