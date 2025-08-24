#include "game.h"
#include "../tracy/public/tracy/Tracy.hpp"

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);

Game::Game()
    : deltaTime(0.0f), isRunning(true), window(nullptr)
{
    this->initWindow();
}

Game::~Game()
{
    cleanup();
}

bool Game::initialize()
{
    // this->initWindow();
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

void Game::update(float deltaTime)
{
    ZoneScoped;
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
    camera.Position += cameraCorrection;
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