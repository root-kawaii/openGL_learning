#pragma once

#include <glad/glad.h>
#include <vector>
#include <queue>
#include <glm/glm.hpp>
#include "../src/game_object.h"
#include <cmath>

// Forward declaration to avoid circular dependency
class Scene;

// Movement command for queued movement system
struct MovementCommand
{
    glm::vec3 destination;
    bool executed = false;
    int ticksRemaining = 0;
};

class GameEntity : public std::enable_shared_from_this<GameEntity>
{
private:
    float timeWaitMilliseconds = 300;
    float timeSinceMovement = 0;

    float speed = 5;
    Scene *scene = nullptr;

    bool hasBall = false;

public:
    GameEntity();
    GameEntity(std::string entityName, std::shared_ptr<GameObject> gameObject);

    std::shared_ptr<GameObject> object;

    // Legacy movement (kept for backward compatibility)
    glm::vec3 targetDestination;
    bool isMoving = false;
    bool hasMovedThisTurn = false;

    // Queued movement system
    std::queue<MovementCommand> movementQueue;
    bool hasCurrentCommand = false;
    MovementCommand currentCommand;

    // Movement methods
    void move(float deltaTime);
    bool executeQueuedMovement(float deltaTime);
    void queueMovement(glm::vec3 destination);
    void clearMovementQueue();
    bool hasQueuedMovements() const { return !movementQueue.empty(); }

    // Helper methods
    bool isGroundHigher(glm::vec3 position);
    bool isGroundLower(glm::vec3 position);
    float distanceFromGameEntity(glm::vec3 position);
    bool isReachable(glm::vec3 position);
    void moveToTarget(GameEntity &targetEntity);
    float findGroundHeight(float x, float z);

    // Scene reference for collision detection
    void setScene(Scene *scenePtr) { scene = scenePtr; }
};