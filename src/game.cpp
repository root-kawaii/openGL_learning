#include "game.h"
#include "../tracy/public/tracy/Tracy.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <../json/single_include/nlohmann/json.hpp>

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);

Game::Game()
    : deltaTime(0.0f), isRunning(true), window(nullptr)
{
    this->initWindow();
    initialize();
    mode = ENGINE;
}

Game::~Game()
{
    cleanup();
}

bool Game::initialize()
{
    // Load settings first
    loadSettings();
    SCR_HEIGHT = settings.resolutionHeight;
    SCR_WIDTH = settings.resolutionWidth;
    std::cout << "Initializing game with resolution: " << SCR_WIDTH << "x" << SCR_HEIGHT << std::endl;
    framebuffer_size_callback(window, settings.resolutionWidth, settings.resolutionHeight);
    uiManager = std::make_shared<UIManager>(settings.resolutionHeight, settings.resolutionWidth);
    uiManager->setWindow(window);
    uiManager->setInputManager(&inputManager);
    auto mainScene = std::make_shared<Scene>();
    this->setScene(mainScene);
    return true;
}

void Game::run()
{
    // TODO: Main game loop
    // while (isRunning) {
    //     calculateDeltaTime();
    //     processInput();
    //     update(deltaTime);
    //     render();
    //     pollEvents();
    // }
}

void Game::update()
{
    ZoneScoped;
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    ImGui::Text("Frametime %f", deltaTime);
    ImGui::Text("FPS %f", 1 / deltaTime);
    lastFrame = currentFrame;

    inputManager.processInput(this, window, &camera, deltaTime, MULTISAMPLE, seed);
    // A temporary place to store all corrections for the frame.
    // This is the key change to prevent cumulative errors.
    glm::vec3 cameraCorrection = glm::vec3(0.0f);

    // --- Phase 1: Object Movement (pre-collision) ---
    // Let's assume the camera's desired movement is also calculated here.
    // For example, based on keyboard input.
    // For now, let's just stick to the objects.
    auto gameObjects = scene->getGameObjects();
    for (auto &obj : gameObjects)
    {
        obj->speed += obj->acceleration * deltaTime;
        // This is the desired position *before* we check for collisions.
        obj->position += obj->speed * deltaTime;
    }

    auto gameEntities = scene->getGameEntities();
    for (auto &entites : gameEntities)
    {
        entites->move(deltaTime);
    }

    // --- Phase 2: Collision Detection and Correction Calculation ---
    // Check all collisions and sum up the required corrections.
    for (size_t m = 0; m < gameObjects.size(); ++m)
    {
        auto &a = gameObjects[m];
        if (a->collisionRadius == 0)
            continue;

        // 1. Calculate camera vs object collision correction.
        // We use the camera's current position and the object's new position
        // to determine if a collision occurred.
        cameraCorrection -= sphereCollision.cameraPositionCorrection(camera, *a);

        // 2. Object-object collision detection and correction.
        // A better approach would be to calculate a correction for both objects (a and b)
        // and store it to be applied later, but we'll stick to a simpler
        // in-loop application for now.
        for (size_t n = m + 1; n < gameObjects.size(); ++n)
        {
            auto &b = gameObjects[n];
            if (b->collisionRadius == 0)
                continue;

            glm::vec3 correction = sphereCollision.simplePositionCorrection(*a, *b);
            if (glm::length(correction) > 0.0f)
            {
                // Apply half the correction to each object to resolve the collision.
                // This is much more stable than applying it to only one.
                a->position += correction * 0.5f;
                b->position -= correction * 0.5f;
            }
        }
    }

    // --- Phase 3: Apply All Final Corrections ---
    // This is the single, final application of the camera correction for the frame.
    handleInput();
    camera.Position += cameraCorrection;

    // --- FPS Limiting to 180 FPS ---
    const float targetFPS = 250.0f;
    const float targetFrameTime = 1.0f / targetFPS;

    float frameEndTime = static_cast<float>(glfwGetTime());
    float frameElapsed = frameEndTime - currentFrame;
    float sleepTime = targetFrameTime - frameElapsed;

    if (sleepTime > 0.0f)
    {
        // Convert to microseconds for more precise sleep
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sleepTime * 1000000.0f)));
    }
}

void Game::loadSettings()
{
    std::ifstream settingsFile("settings/settings.json");
    if (!settingsFile.is_open())
    {
        std::cerr << "Failed to open settings file. Using default settings." << std::endl;
        return;
    }

    nlohmann::json settingsJson;
    try
    {
        settingsFile >> settingsJson;
    }
    catch (const nlohmann::json::parse_error &e)
    {
        std::cerr << "Error parsing settings JSON: " << e.what() << std::endl;
        return;
    }

    std::cout << "Loading settings from settings.json..." << std::endl;

    // Load graphics settings
    if (settingsJson.contains("graphics"))
    {
        const auto &graphics = settingsJson["graphics"];

        if (graphics.contains("resolution"))
        {
            const auto &resolution = graphics["resolution"];
            settings.resolutionWidth = resolution.value("width", settings.resolutionWidth);
            settings.resolutionHeight = resolution.value("height", settings.resolutionHeight);
            settings.fullscreen = resolution.value("fullscreen", settings.fullscreen);

            std::cout << "  Resolution: " << settings.resolutionWidth << "x" << settings.resolutionHeight
                      << (settings.fullscreen ? " (Fullscreen)" : " (Windowed)") << std::endl;
        }
    }

    // Load audio settings
    if (settingsJson.contains("audio"))
    {
        const auto &audio = settingsJson["audio"];
        settings.masterVolume = audio.value("masterVolume", settings.masterVolume);

        std::cout << "  Master Volume: " << settings.masterVolume << std::endl;

        // TODO: Apply audio settings when AudioManager has setMasterVolume method
        // audioManager.setMasterVolume(settings.masterVolume);
    }

    // Load gameplay settings
    if (settingsJson.contains("gameplay"))
    {
        const auto &gameplay = settingsJson["gameplay"];
        settings.difficulty = gameplay.value("difficulty", settings.difficulty);
        settings.autosave = gameplay.value("autosave", settings.autosave);
        settings.autosaveInterval = gameplay.value("autosaveInterval", settings.autosaveInterval);

        std::cout << "  Difficulty: " << settings.difficulty << std::endl;
        std::cout << "  Autosave: " << (settings.autosave ? "Enabled" : "Disabled");
        if (settings.autosave)
            std::cout << " (every " << settings.autosaveInterval << "s)";
        std::cout << std::endl;
    }

    // Load controls settings
    if (settingsJson.contains("controls"))
    {
        const auto &controls = settingsJson["controls"];
        settings.invertY = controls.value("invertY", settings.invertY);

        std::cout << "  Invert Y-Axis: " << (settings.invertY ? "Yes" : "No") << std::endl;
    }

    std::cout << "Settings loaded successfully!" << std::endl;
}

void Game::render()
{
    // TODO: Render frame
    // renderManager.beginFrame();
    // renderManager.clear();
    // sceneManager.render();
    // renderManager.renderScene();
    // renderManager.endFrame();
    // renderManager.present();
}

void Game::cleanup()
{
    // TODO: Cleanup all resources
    // TODO: Destroy managers
    // TODO: Close window and terminate GLFW
}

// void Game::processInput()
// {
//     // TODO: Process input events
//     // inputManager.update();
//     // TODO: Handle game-specific input (pause, quit, etc.)
// }

// void Game::calculateDeltaTime()
// {
//     // TODO: Calculate time between frames
//     // static float lastFrame = 0.0f;
//     // float currentFrame = glfwGetTime();
//     // deltaTime = currentFrame - lastFrame;
//     // lastFrame = currentFrame;
// }

// void Game::pollEvents()
// {
//     // TODO: Poll window events
//     // glfwPollEvents();
//     // TODO: Check if window should close
// }

// void Game::setState(GameState newState)
// {
//     // TODO: Handle state transitions
//     // TODO: Cleanup current state
//     // TODO: Initialize new state
//     currentState = newState;
// }

// void Game::pause()
// {
//     // TODO: Pause game systems
//     // audioManager.pauseAll();
//     // TODO: Set paused state
// }

// void Game::resume()
// {
//     // TODO: Resume game systems
//     // audioManager.resumeAll();
//     // TODO: Reset delta time to avoid large jump
// }

// void Game::quit()
// {
//     // TODO: Initiate shutdown sequence
//     // TODO: Save game state if needed
//     isRunning = false;
// }

// EntityManager& Game::getEntityManager()
// {
//     // TODO: Return reference to entity manager
//     return entityManager;
// }

// RenderManager& Game::getRenderManager()
// {
//     // TODO: Return reference to render manager
//     return renderManager;
// }

// InputManager& Game::getInputManager()
// {
//     // TODO: Return reference to input manager
//     return inputManager;
// }

// AudioManager& Game::getAudioManager()
// {
//     // TODO: Return reference to audio manager
//     return audioManager;
// }

// SceneManager& Game::getSceneManager()
// {
//     // TODO: Return reference to scene manager
//     return sceneManager;
// }

// PhysicsManager& Game::getPhysicsManager()
// {
//     // TODO: Return reference to physics manager
//     return physicsManager;
// }

// ResourceManager& Game::getResourceManager()
// {
//     // TODO: Return reference to resource manager
//     return resourceManager;
// }

// GameState Game::getCurrentState() const
// {
//     return currentState;
// }

// float Game::getDeltaTime() const
// {
//     return deltaTime;
// }

// bool Game::isGameRunning() const
// {
//     return isRunning;
// }

// GLFWwindow* Game::getWindow()
// {
//     return window;
// }

// void Game::onWindowResize(int width, int height)
// {
//     // TODO: Handle window resize
//     // renderManager.setViewport(width, height);
//     // TODO: Update camera aspect ratio
// }

// void Game::onWindowClose()
// {
//     // TODO: Handle window close event
//     quit();
// }

// void Game::loadSettings()
// {
//     // TODO: Load game settings from file
//     // TODO: Apply settings to managers
// }

// void Game::saveSettings()
// {
//     // TODO: Save current settings to file
// }

// void Game::loadGameData()
// {
//     // TODO: Load saved game data
// }

// void Game::saveGameData()
// {
//     // TODO: Save current game progress
// }

void Game::processGameInput(GLFWwindow *window, Camera *camera, float deltaTime, bool &shadows, float &seed)
{
    inputManager.processInput(this, window, camera, deltaTime, shadows, seed);
}

void Game::setLevel(std::string levelName)
{
    scene = std::make_unique<Scene>(levelName);
}

std::unordered_map<int, bool> previousKeyStates;

bool wasKeyJustPressed(int key, GLFWwindow *window)
{
    bool currentlyPressed = (glfwGetKey(window, key) == GLFW_PRESS);
    bool wasPressed = previousKeyStates[key];

    previousKeyStates[key] = currentlyPressed;

    return currentlyPressed && !wasPressed;
}

void Game::handleInput()
{
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && wasKeyJustPressed(GLFW_KEY_C, window))
    {
        if (scene->getSelectedGameObject() == nullptr)
        {
            return;
        }
        scene->duplicateGameObject(scene->getSelectedGameObject()->ID);
    }
}
