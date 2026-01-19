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
    players.push_back(Player("Player 1"));
    SCR_HEIGHT = settings.resolutionHeight;
    SCR_WIDTH = settings.resolutionWidth;
    std::cout << "Initializing game with resolution: " << SCR_WIDTH << "x" << SCR_HEIGHT << std::endl;
    framebuffer_size_callback(window, settings.resolutionWidth, settings.resolutionHeight);
    uiManager = std::make_shared<UIManager>(settings.resolutionHeight, settings.resolutionWidth);
    uiManager->setWindow(window);
    uiManager->setInputManager(&inputManager);
    uiManager->setGame(this);
    auto mainScene = std::make_shared<Scene>(&renderManager);
    this->setScene(mainScene);
    scene->setGame(this);
    scene->setUIManager(uiManager.get());
    renderManager.setGame(this);
    renderManager.setUIManager(uiManager.get());

    // Initialize turn system - player-based, not entity-based
    // All entities start with hasMovedThisTurn = false
    resetAllEntityMovement();

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

    inputManager.processInput(this, window, &camera, deltaTime, MULTISAMPLE, seed, &renderManager);

    // --- Phase 1: Object Movement (pre-collision) ---
    auto gameObjects = scene->getGameObjects();
    for (auto &obj : gameObjects)
    {
        obj->speed += obj->acceleration * deltaTime;
        // This is the desired position *before* we check for collisions.
        obj->position += obj->speed * deltaTime;
    }

    // Update queued movement system
    updateTurnExecution();

    // Update ball flight trajectory
    for (auto &entity : scene->getGameEntities())
    {
        entity->updateBallFlight(deltaTime);
    }

    // --- Animation System Test: Switch between animations every 3 seconds ---
    static float animationTimer = 0.0f;
    static bool isPlayingBounce = false;
    animationTimer += deltaTime;

    if (animationTimer >= 1.0f)
    {
        animationTimer = 0.0f;
        isPlayingBounce = !isPlayingBounce;

        // Switch animation on all mech_drone models
        for (auto &obj : gameObjects)
        {
            if (obj->modelPath.find("mech_drone_multi") != std::string::npos)
            {
                if (isPlayingBounce)
                {
                    std::cout << "[Animation] Switching to Bounce animation with 0.5s blend" << std::endl;
                    obj->model.PlayAnimation("Bounce", 0.5f);
                }
                else
                {
                    std::cout << "[Animation] Switching to Take 001 animation with 0.5s blend" << std::endl;
                    obj->model.PlayAnimation("Take 001", 0.5f);
                }
            }
        }
    }

    // --- Phase 2: Optimized Collision Detection ---
    // Using new CollisionSystem for better performance
    // glm::vec3 cameraCorrection = collisionSystem.performCollisionPass(camera, gameObjects);

    // --- Phase 3: Apply All Final Corrections ---
    handleInput();
    // camera.Position += cameraCorrection;

    handleTurn();

    // --- FPS Limiting to 180 FPS ---
    const float targetFPS = 250.0f;
    const float targetFrameTime = 1.0f / targetFPS;

    float frameEndTime = static_cast<float>(glfwGetTime());
    float frameElapsed = frameEndTime - currentFrame;
    float sleepTime = targetFrameTime - frameElapsed;

    if (sleepTime > 0.0f)
    {
        // Convert to microseconds for more precise sleep
        // std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sleepTime * 1000000.0f)));
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

void Game::processGameInput(GLFWwindow *window, Camera *camera, float deltaTime, bool &shadows, float &seed, RenderManager *renderManager)
{
    inputManager.processInput(this, window, camera, deltaTime, shadows, seed, renderManager);
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

    // Shoot ball with 'F' key
    if (wasKeyJustPressed(GLFW_KEY_F, window))
    {
        // Find entity with the ball and shoot
        for (auto &entity : scene->getGameEntities())
        {
            if (entity->getHasBall())
            {
                entity->shootBall(glm::vec3(-2.45f, 2.85f, 0.18f));
                break;
            }
        }
    }

    // Pass ball with 'G' key
    if (wasKeyJustPressed(GLFW_KEY_G, window))
    {
        // Find entity with the ball
        std::shared_ptr<GameEntity> ballHolder = nullptr;
        for (auto &entity : scene->getGameEntities())
        {
            if (entity->getHasBall())
            {
                ballHolder = entity;
                break;
            }
        }

        if (ballHolder)
        {
            // Find another capsule entity to pass to
            for (auto &entity : scene->getGameEntities())
            {
                // Pass to any other capsule entity (not the one holding the ball)
                if (entity != ballHolder &&
                    entity->object->name.find("capsule") != std::string::npos)
                {
                    ballHolder->passBall(entity.get());
                    break;
                }
            }
        }
    }

    // Add cube below selected entity with 'V' key
    if (wasKeyJustPressed(GLFW_KEY_V, window))
    {
        if (selectedEntity)
        {
            scene->addCubeBelowEntity(selectedEntity, "simple_color_shader");
        }
    }
}

void Game::handleTurn()
{
    // if (getGameMode() != GAME)
    // {
    //     return;
    // }
    auto gameEntities = scene->getGameEntities();
    for (auto &entity : gameEntities)
    {
        // entity->onNewTurn();
    }
}

// Check if entity can be selected based on tag
bool Game::isEntitySelectable(std::shared_ptr<GameEntity> entity)
{
    return true;
    if (!entity || !entity->object)
        return false;
    std::string entityTag = entity->object->gameEntity;

    for (const auto &tag : selectableEntityTags)
    {
        if (entityTag.find(tag) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

// Check if entity can move this turn (hasn't moved yet)
bool Game::canEntityMove(std::shared_ptr<GameEntity> entity)
{
    if (!entity)
        return false;
    return !entity->hasMovedThisTurn;
}

// Set the currently selected entity
void Game::setSelectedEntity(std::shared_ptr<GameEntity> entity)
{
    selectedEntity = entity;
    if (entity && entity->object)
    {
        scene->setSelectedObject(entity->object);
        std::cout << "Selected entity: " << entity->object->name << std::endl;
    }
}

// Reset all entity movement flags (called at end of turn)
void Game::resetAllEntityMovement()
{
    auto entities = scene->getGameEntities();
    for (auto &entity : entities)
    {
        entity->hasMovedThisTurn = false;
    }
    std::cout << "All entity movement flags reset" << std::endl;
}

// Hash function for glm::ivec3
struct ivec3Hash
{
    size_t operator()(const glm::ivec3 &v) const
    {
        return std::hash<int>()(v.x) ^
               (std::hash<int>()(v.y) << 1) ^
               (std::hash<int>()(v.z) << 2);
    }
};

// Helper to discretize position to grid
glm::ivec3 discretizeToGrid(const glm::vec3 &pos)
{
    return glm::ivec3(std::floor(pos.x), std::floor(pos.y), std::floor(pos.z));
}

// Start turn execution - trigger all queued movements
void Game::startTurnExecution()
{
    if (turnState != TurnState::PLANNING)
    {
        std::cout << "[Turn] Cannot start execution - not in planning phase" << std::endl;
        return;
    }

    std::cout << "\n=== TURN EXECUTION START ===" << std::endl;
    turnState = TurnState::EXECUTING;
    allMovementsComplete = false;

    // Count queued movements
    int queuedCount = 0;
    for (auto &entity : scene->getGameEntities())
    {
        if (entity->hasQueuedMovements())
            queuedCount++;
    }
    std::cout << "[Turn] " << queuedCount << " entities with queued movements" << std::endl;
}

// Update turn execution - process movements and detect collisions
void Game::updateTurnExecution()
{
    if (turnState != TurnState::EXECUTING)
        return;

    // Track which entities moved this tick
    std::vector<std::pair<std::shared_ptr<GameEntity>, glm::vec3>> tickMovements;

    // Execute one step for each entity
    for (auto &entity : scene->getGameEntities())
    {

        glm::vec3 oldPos = entity->object->position;
        bool moved = entity->executeQueuedMovement(deltaTime);

        if (moved)
        {
            tickMovements.push_back({entity, oldPos});
        }
    }

    // Collision detection
    if (!tickMovements.empty())
    {
        detectCollisions(tickMovements);
    }

    // Check if all movements complete
    if (checkAllMovementsComplete())
    {
        std::cout << "=== TURN EXECUTION COMPLETE ===" << std::endl;
        turnState = TurnState::PLANNING;
        allMovementsComplete = true;
    }
}

// Check if all entities have completed their movements
bool Game::checkAllMovementsComplete()
{
    for (auto &entity : scene->getGameEntities())
    {
        if (entity->hasQueuedMovements() || entity->hasCurrentCommand)
        {
            return false;
        }
    }
    return true;
}

// Detect collisions between entities
void Game::detectCollisions(const std::vector<std::pair<std::shared_ptr<GameEntity>, glm::vec3>> &tickMovements)
{
    // Build arrivals map: destination -> [entities]
    std::unordered_map<glm::ivec3, std::vector<std::shared_ptr<GameEntity>>, ivec3Hash> arrivals;

    // Track movements for crossing detection
    struct Movement
    {
        std::shared_ptr<GameEntity> entity;
        glm::ivec3 from;
        glm::ivec3 to;
    };
    std::vector<Movement> movements;

    // Gather all movements this tick
    for (const auto &[entity, oldPos] : tickMovements)
    {
        glm::ivec3 fromGrid = discretizeToGrid(oldPos);
        glm::ivec3 toGrid = discretizeToGrid(entity->object->position);

        arrivals[toGrid].push_back(entity);
        movements.push_back({entity, fromGrid, toGrid});
    }

    // 1. Detect same-cell collisions (2+ entities at same position)
    for (const auto &[cell, entities] : arrivals)
    {
        if (entities.size() > 1)
        {
            std::cout << "[Collision] Same-cell: " << entities.size()
                      << " entities at (" << cell.x << ", " << cell.y << ", " << cell.z << ")" << std::endl;
            handleSameCellCollision(cell, entities);
        }
    }

    // 2. Detect crossing collisions (A→B while B→A)
    for (size_t i = 0; i < movements.size(); i++)
    {
        for (size_t j = i + 1; j < movements.size(); j++)
        {
            const auto &a = movements[i];
            const auto &b = movements[j];

            // Check if they swapped positions
            if (a.from == b.to && a.to == b.from)
            {
                std::cout << "[Collision] Crossing: entities swapped positions" << std::endl;
                handleCrossingCollision(a.entity, b.entity);
            }
        }
    }
}

// Handle same-cell collision - stack entities vertically
void Game::handleSameCellCollision(glm::ivec3 cell, const std::vector<std::shared_ptr<GameEntity>> &entities)
{
    // Stack entities vertically
    // For now: random order (future: use weight field)
    float stackHeight = 0.0f;

    for (auto &entity : entities)
    {
        // Set Y position based on stack order
        glm::vec3 pos = entity->object->position;
        pos.y = static_cast<float>(cell.y) + stackHeight;
        entity->object->position = pos;

        std::cout << "[Stacking] Entity at height " << stackHeight << std::endl;

        // TODO: Play jump animation
        // entity->object->model.PlayAnimation("Jump");

        stackHeight += 1.0f;
    }

    // Update scene occupancy map
    scene->updateOccupancyAfterCollision(cell, entities);
}

// Handle crossing collision - entities swap positions
void Game::handleCrossingCollision(std::shared_ptr<GameEntity> entityA, std::shared_ptr<GameEntity> entityB)
{
    // TODO: Play crossing animation
    std::cout << "[Crossing] Entities crossed paths" << std::endl;
    // Could play a "dodge" or "bump" animation
}

// End player turn and advance to next turn
void Game::endPlayerTurn()
{
    std::cout << "=== Ending Player Turn ===" << std::endl;

    // Reset turn state to planning
    turnState = TurnState::PLANNING;
    allMovementsComplete = true;

    // Clear all queued movements
    for (auto &entity : scene->getGameEntities())
    {
        entity->clearMovementQueue();
    }

    // Reset all entity movement flags
    resetAllEntityMovement();

    // Increment turn counter
    turn++;
    std::cout << "\n=== TURN " << turn << " ===" << std::endl;

    // Clear selection
    selectedEntity = nullptr;
    scene->setSelectedObject(nullptr);
}