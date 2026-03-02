#pragma once

#include <glad/glad.h>
#include <vector>
#include <queue>
#include <string>
#include <glm/glm.hpp>
#include "../src/game_object.h" // Includes AABB
#include "entity_class.h"
#include <cmath>

// Forward declaration to avoid circular dependency
class Scene;
class GameEntity;

// Action types for the buffered action system
enum class ActionType
{
    MOVE,
    SHOOT,
    PASS,
    TACKLE,  // Steal the ball from an adjacent enemy (strength contest)
};

struct BufferedAction
{
    ActionType type;
    glm::vec3 targetPosition = glm::vec3(0.0f);
    GameEntity *targetEntity = nullptr;
    std::string description;
};

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

    Scene *scene = nullptr;

    bool hasBall = false;

    // Ball shooting state
    bool isBallFlying = false;
    bool isLinearTrajectory = false; // true for pass (straight line), false for shoot (parabolic)
    glm::vec3 ballStartPos;
    glm::vec3 ballEndPos = glm::vec3(5.0f, 5.0f, 5.0f);
    float ballFlightTime = 0.0f;
    float ballFlightDuration = 1.5f; // Time in seconds for ball to reach target
    GameEntity* passTarget = nullptr; // Entity we're passing to
    bool shotWillScore = false; // Computed at shoot time based on accuracy + distance
    bool isRebounding = false;  // True during the rebound after a missed shot

public:
    GameEntity();
    GameEntity(std::string entityName, std::shared_ptr<GameObject> gameObject);
    GameEntity(std::string entityName, std::shared_ptr<GameObject> gameObject, EntityClassType classType);

    std::shared_ptr<GameObject> object;
    EntityClass entityClass;

    // Legacy movement (kept for backward compatibility)
    glm::vec3 targetDestination;
    bool isMoving = false;
    bool hasMovedThisTurn = false;

    // Per-turn state
    bool isTired      = false; // true if sprinted last turn → half movement range
    bool sprintQueued = false; // toggled by R key → double movement range this turn
    bool isTackling   = false; // set when TACKLE action queued — triggers on adjacency during movement

    // Queued movement system
    std::queue<MovementCommand> movementQueue;
    bool hasCurrentCommand = false;
    MovementCommand currentCommand;

    // Movement methods
    void move(float deltaTime);
    bool executeQueuedMovement(float deltaTime);
    void queueMovement(glm::vec3 destination);
    void clearMovementQueue();
    bool hasQueuedMovements() const { return !movementQueue.empty() || hasCurrentCommand; }
    glm::vec3 getQueuedDestination() const;

    // Helper methods
    bool isGroundHigher(glm::vec3 position);
    bool isGroundLower(glm::vec3 position);
    float distanceFromGameEntity(glm::vec3 position);
    bool isReachable(glm::vec3 position);
    void moveToTarget(GameEntity &targetEntity);
    float findGroundHeight(float x, float z);

    // Scene reference for collision detection
    void setScene(Scene *scenePtr) { scene = scenePtr; }

    // Movement helpers
    int  getEffectiveMovementSpeed() const;

    // Ball control
    bool getHasBall() const { return hasBall; }
    void setHasBall(bool has) { hasBall = has; }
    void shootBall(glm::vec3 target);
    void passBall(GameEntity* targetEntity);
    void updateBallFlight(float deltaTime);
    bool isBallInFlight() const { return isBallFlying; }

    // New actions
    // tackle(): called by game loop when this entity steps adjacent to `target` while isTackling.
    // Returns true = steal succeeded; false = ball knocked loose (caller handles repositioning).
    bool tackle(GameEntity* target);
    void onTurnEnd(); // reset per-turn flags (called when EXECUTING→PLANNING)

    // Action buffer system
    std::vector<BufferedAction> actionBuffer;
    bool bufferAction(const BufferedAction &action);
    void removeAction(int index);
    void clearActions();
    const std::vector<BufferedAction> &getActionBuffer() const { return actionBuffer; }
    bool hasBufferedMove() const;
    bool hasBufferedNonMove() const;

private:
    // Ball collision detection
    bool checkBallCollision(glm::vec3 ballPos, float ballRadius, glm::vec3& hitNormal);
    bool sphereAABBCollision(glm::vec3 sphereCenter, float radius, const AABB& box, glm::vec3& hitNormal);
};