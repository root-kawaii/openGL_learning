#include "game_entity.h"
#include "projectile_bounce_utils.h"
#include "scene.h"
#include <thread>
#include <chrono>
#include <random>

GameEntity::GameEntity()
{
}

GameEntity::GameEntity(std::string entityName, std::shared_ptr<GameObject> gameObject)
{
    object = gameObject;
    object->gameEntity = entityName;
    entityClass = EntityClass::fromType(EntityClassType::DEFAULT);
}

GameEntity::GameEntity(std::string entityName, std::shared_ptr<GameObject> gameObject, EntityClassType classType)
{
    object = gameObject;
    object->gameEntity = entityName;
    entityClass = EntityClass::fromType(classType);
}

void GameEntity::queueMovement(glm::vec3 destination)
{
    MovementCommand cmd;
    cmd.destination = destination;
    cmd.executed = false;

    // Calculate ticks needed (Manhattan distance)
    float distance = std::abs(destination.x - object->position.x) +
                     std::abs(destination.z - object->position.z);
    cmd.ticksRemaining = static_cast<int>(std::ceil(distance));

    movementQueue.push(cmd);
    std::cout << "[Queue] Added movement to (" << destination.x << ", "
              << destination.y << ", " << destination.z << "), "
              << cmd.ticksRemaining << " ticks" << std::endl;
}

void GameEntity::clearMovementQueue()
{
    while (!movementQueue.empty())
    {
        movementQueue.pop();
    }
    hasCurrentCommand = false;
}

glm::vec3 GameEntity::getQueuedDestination() const
{
    if (hasCurrentCommand)
    {
        return currentCommand.destination;
    }
    if (!movementQueue.empty())
    {
        // Return the last destination in the queue (final position)
        std::queue<MovementCommand> tempQueue = movementQueue;
        MovementCommand lastCmd;
        while (!tempQueue.empty())
        {
            lastCmd = tempQueue.front();
            tempQueue.pop();
        }
        return lastCmd.destination;
    }
    return object->position; // No queued movement, return current position
}

// Helper function to find the highest Y position at a given X,Z coordinate
float GameEntity::findGroundHeight(float x, float z)
{
    float highestY = 0.5f; // Minimum ground level is 0.5

    if (!scene)
        return highestY;

    // Check all game objects in the scene
    for (const auto &obj : scene->getGameObjects())
    {
        if (obj.get() == object.get())
            continue; // Skip self
        if (scene->ball && obj.get() == scene->ball)
            continue; // Skip ball

        // Check if this object is at the same X,Z position (discretized)
        float objX = std::floor(obj->position.x);
        float objZ = std::floor(obj->position.z);
        float targetX = std::floor(x);
        float targetZ = std::floor(z);

        if (objX == targetX && objZ == targetZ)
        {
            // Object is at this X,Z - check its height (positions are at 0.5 increments)
            float objTop = obj->position.y + 1.0f; // Top of the object
            if (objTop > highestY)
            {
                highestY = objTop;
            }
        }
    }

    return highestY;
}

bool GameEntity::executeQueuedMovement(float deltaTime)
{
    // Check cooldown
    if (timeSinceMovement < timeWaitMilliseconds)
    {
        timeSinceMovement += deltaTime * 1000;
        return false; // Not ready to move yet
    }

    // Get current command
    if (!hasCurrentCommand && !movementQueue.empty())
    {
        // Pop next command from queue
        currentCommand = movementQueue.front();
        movementQueue.pop();
        hasCurrentCommand = true;
    }

    if (!hasCurrentCommand)
        return false; // No commands

    const float epsilon = 0.01f;
    glm::vec3 &targetDest = currentCommand.destination;

    // Check if reached destination (only check X and Z)
    float xzDistance = std::sqrt(
        std::pow(targetDest.x - object->position.x, 2) +
        std::pow(targetDest.z - object->position.z, 2));
    if (xzDistance <= epsilon)
    {
        std::cout << "[Movement] Entity reached destination" << std::endl;
        currentCommand.executed = true;
        hasCurrentCommand = false;
        return true; // Movement completed
    }

    // Calculate next step
    float x_distance = targetDest.x - object->position.x;
    float z_distance = targetDest.z - object->position.z;

    // Priority: X-axis first
    if (std::abs(x_distance) > epsilon)
    {
        float x_step = (x_distance > 0) ? 1.0f : -1.0f;
        float newX = object->position.x + x_step;
        float newZ = object->position.z;

        // Find the ground height at the new position
        float groundY = findGroundHeight(newX, newZ);
        glm::vec3 movement = glm::vec3(newX, groundY, newZ) - object->position;
        object->position = glm::vec3(newX, groundY, newZ);

        // Move ball with entity if we have it
        if (hasBall && scene && scene->ball)
        {
            scene->ball->position += movement;
        }

        timeSinceMovement = 0;
        return true; // Moved this tick
    }

    // Z-axis movement
    if (std::abs(z_distance) > epsilon)
    {
        float z_step = (z_distance > 0) ? 1.0f : -1.0f;
        float newX = object->position.x;
        float newZ = object->position.z + z_step;

        // Find the ground height at the new position
        float groundY = findGroundHeight(newX, newZ);
        glm::vec3 movement = glm::vec3(newX, groundY, newZ) - object->position;
        object->position = glm::vec3(newX, groundY, newZ);

        // Move ball with entity if we have it
        if (hasBall && scene && scene->ball)
        {
            scene->ball->position += movement;
        }

        timeSinceMovement = 0;
        return true; // Moved this tick
    }

    return false;
}

void GameEntity::move(float deltaTime)
{

    if (timeSinceMovement < timeWaitMilliseconds)
    {
        timeSinceMovement += deltaTime * 1000;
        return;
    }

    if (!isMoving)
        return;

    const float epsilon = 0.01f;

    if (glm::distance(object->position, targetDestination) <= epsilon)
    {
        isMoving = false;
        return;
    }

    float x_distance = targetDestination.x - object->position.x;
    float z_distance = targetDestination.z - object->position.z;

    if (std::abs(x_distance) > epsilon)
    {
        float x_step = (x_distance > 0) ? 1.0f : -1.0f;
        glm::vec3 next_pos = object->position + glm::vec3(x_step, 0, 0);

        if (isGroundHigher(next_pos))
        {

            if (hasBall)
            {
                scene->ball->position += glm::vec3(x_step, 1, 0);
            }
            object->position += glm::vec3(x_step, 1, 0);
        }
        else if (isGroundLower(next_pos))
        {
            if (hasBall)
            {
                scene->ball->position += glm::vec3(x_step, -1, 0);
            }
            object->position += glm::vec3(x_step, -1, 0);
        }
        else
        {
            if (hasBall)
            {
                scene->ball->position += glm::vec3(x_step, 0, 0);
            }
            object->position += glm::vec3(x_step, 0, 0);
        }
        timeSinceMovement = 0;
        return;
    }

    if (std::abs(z_distance) > epsilon)
    {
        float z_step = (z_distance > 0) ? 1.0f : -1.0f;
        glm::vec3 next_pos = object->position + glm::vec3(0, 0, z_step);

        // fix ball, plus we need animation so depening on that

        if (isGroundHigher(next_pos))
        {
            if (hasBall)
            {
                scene->ball->position += glm::vec3(0, 1, z_step);
            }
            object->position += glm::vec3(0, 1, z_step);
        }
        else if (isGroundLower(next_pos))
        {
            if (hasBall)
            {
                scene->ball->position += glm::vec3(0, -1, z_step);
            }
            object->position += glm::vec3(0, -1, z_step);
        }
        else
        {
            if (hasBall)
            {
                scene->ball->position += glm::vec3(0, 0, z_step);
            }
            object->position += glm::vec3(0, 0, z_step);
        }
        timeSinceMovement = 0;
    }
    timeSinceMovement = 0;
}

bool GameEntity::isGroundHigher(glm::vec3 position)
{
    return false;
}

bool GameEntity::isGroundLower(glm::vec3 position)
{
    return false;
}

float GameEntity::distanceFromGameEntity(glm::vec3 position)
{
    float obj_x = std::floor(object->position.x) + 0.5f;
    float obj_y = std::floor(object->position.y) + 0.5f;
    float obj_z = std::floor(object->position.z) + 0.5f;
    float x_distance = std::fabs(obj_x - position.x); // Changed
    float y_distance = std::fabs(obj_y - position.y); // Changed
    float z_distance = std::fabs(obj_z - position.z); // Changed
    return x_distance + y_distance + z_distance;
}

bool GameEntity::isReachable(glm::vec3 position)
{
    if (distanceFromGameEntity(position) <= entityClass.movementSpeed)
        return true;
    return false;
}

void GameEntity::moveToTarget(GameEntity &targetEntity)
{
    targetDestination = targetEntity.object->position;
    isMoving = true;
}

void GameEntity::shootBall(glm::vec3 target)
{
    if (!hasBall || !scene || !scene->ball)
        return;

    // Release the ball
    hasBall = false;
    isBallFlying = true;
    isLinearTrajectory = false; // Parabolic arc for shooting
    ballStartPos = scene->ball->position;
    ballEndPos = target;
    ballFlightTime = 0.0f;
    // Higher strength = faster shot (base 1.5s, scaled down by strength)
    ballFlightDuration = 1.5f * (5.0f / static_cast<float>(entityClass.strength));
    passTarget = nullptr; // Not a pass, just a shoot

    // Compute shot accuracy based on accuracy stat and distance
    float distance = glm::distance(object->position, target);
    float baseChance = static_cast<float>(entityClass.accuracy) / 10.0f;
    float probability = glm::clamp(baseChance - (distance * 0.03f), 0.05f, 0.95f);

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> rollDist(0.0f, 1.0f);
    float roll = rollDist(rng);
    shotWillScore = (roll < probability);
    isRebounding = false;

    if (!shotWillScore)
    {
        // Offset the target so ball hits the rim instead of going in
        std::uniform_real_distribution<float> offsetDist(0.5f, 1.5f);
        std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
        float offsetMag = offsetDist(rng);
        float angle = angleDist(rng);
        ballEndPos.x += offsetMag * cos(angle);
        ballEndPos.z += offsetMag * sin(angle);
        std::cout << "[Shoot] MISS (prob=" << probability << ", roll=" << roll << ") offset target to ("
                  << ballEndPos.x << ", " << ballEndPos.y << ", " << ballEndPos.z << ")" << std::endl;
    }
    else
    {
        std::cout << "[Shoot] ON TARGET (prob=" << probability << ", roll=" << roll << ")" << std::endl;
    }

    std::cout << "[Shoot] Ball shot from (" << ballStartPos.x << ", " << ballStartPos.y << ", " << ballStartPos.z
              << ") to (" << ballEndPos.x << ", " << ballEndPos.y << ", " << ballEndPos.z << ")" << std::endl;
}

void GameEntity::passBall(GameEntity *targetEntity)
{
    if (!hasBall || !scene || !scene->ball || !targetEntity)
        return;

    // Release the ball
    hasBall = false;
    isBallFlying = true;
    isLinearTrajectory = true; // Straight line for passing
    ballStartPos = scene->ball->position;
    // Pass to the target entity's position (slightly above)
    ballEndPos = targetEntity->object->position + glm::vec3(0.0f, 0.5f, 0.0f);
    ballFlightTime = 0.0f;
    // Higher strength = faster pass (base 0.8s, scaled down by strength)
    ballFlightDuration = 0.8f * (5.0f / static_cast<float>(entityClass.strength));
    passTarget = targetEntity; // Remember who we're passing to

    std::cout << "[Pass] Ball passed from (" << ballStartPos.x << ", " << ballStartPos.y << ", " << ballStartPos.z
              << ") to " << targetEntity->object->name << " at (" << ballEndPos.x << ", " << ballEndPos.y << ", " << ballEndPos.z << ")" << std::endl;
}

void GameEntity::updateBallFlight(float deltaTime)
{
    if (!isBallFlying || !scene || !scene->ball)
        return;

    ballFlightTime += deltaTime;
    float t = ballFlightTime / ballFlightDuration;

    if (t >= 1.0f)
    {
        // Ball reached destination
        t = 1.0f;
        isBallFlying = false;

        // If this was a pass, ball lands at target position (pickup system handles the rest)
        if (passTarget != nullptr)
        {
            scene->ball->position = ballEndPos;
            std::cout << "[Pass] Ball arrived near " << passTarget->object->name << std::endl;
            passTarget = nullptr;
        }
        else if (shotWillScore)
        {
            // GOAL! Respawn ball at arena center
            glm::vec3 arenaCenter(0.0f, 0.5f, 0.0f);
            scene->ball->position = arenaCenter;
            std::cout << "[GOAL!] Ball respawned at arena center" << std::endl;
            shotWillScore = false;
        }
        else if (!isRebounding)
        {
            // Miss — apply rebound from target position
            scene->ball->position = ballEndPos;

            // Random rebound direction based on incoming direction
            glm::vec3 incomingDir = glm::normalize(ballEndPos - ballStartPos);
            static std::mt19937 bounceRng(std::random_device{}());
            std::uniform_real_distribution<float> deviationDist(-0.5f, 0.5f);
            float deviationAngle = deviationDist(bounceRng);
            float cosA = cos(deviationAngle);
            float sinA = sin(deviationAngle);
            // Reflect roughly back and deviate randomly around Y
            float rx = -(incomingDir.x * cosA - incomingDir.z * sinA);
            float rz = -(incomingDir.x * sinA + incomingDir.z * cosA);
            glm::vec2 reboundDir2 = glm::normalize(glm::vec2(rx, rz));

            // Rebound distance scales with shot distance
            float totalDist = glm::distance(ballStartPos, ballEndPos);
            float reboundDist = totalDist * 0.3f;

            ballStartPos = ballEndPos;
            ballEndPos = ballStartPos + glm::vec3(reboundDir2.x, 0.0f, reboundDir2.y) * reboundDist;
            ballEndPos.y = 0.5f; // Ground level
            isLinearTrajectory = true;
            ballFlightTime = 0.0f;
            ballFlightDuration = 0.6f; // Short rebound duration
            isBallFlying = true;
            isRebounding = true;

            std::cout << "[Shoot] Ball missed, rebounding to (" << ballEndPos.x << ", " << ballEndPos.y << ", " << ballEndPos.z << ")" << std::endl;
        }
        else
        {
            // Rebound finished — ball lands
            scene->ball->position = ballEndPos;
            isRebounding = false;
            std::cout << "[Shoot] Ball landed at (" << ballEndPos.x << ", " << ballEndPos.y << ", " << ballEndPos.z << ")" << std::endl;
        }
        return;
    }

    // Linear interpolation for X and Z
    float x = ballStartPos.x + t * (ballEndPos.x - ballStartPos.x);
    float z = ballStartPos.z + t * (ballEndPos.z - ballStartPos.z);
    float y;

    if (isLinearTrajectory)
    {
        // Straight line for passes
        y = ballStartPos.y + t * (ballEndPos.y - ballStartPos.y);
    }
    else
    {
        // Parabolic arc for shoots
        // y(t) = startY + t*(endY - startY) + 4*h*t*(1-t) where h is the peak height above the linear path
        float linearY = ballStartPos.y + t * (ballEndPos.y - ballStartPos.y);
        float peakHeight = 3.0f; // Height of the arc above the linear path
        float parabolicOffset = 4.0f * peakHeight * t * (1.0f - t);
        y = linearY + parabolicOffset;
    }

    glm::vec3 newPos = glm::vec3(x, y, z);
    scene->ball->position = newPos;
}

bool GameEntity::sphereAABBCollision(glm::vec3 sphereCenter, float radius, const AABB &box, glm::vec3 &hitNormal)
{
    return ProjectileBounceUtils::sphereIntersectsAabb(sphereCenter, radius, box, hitNormal);
}

bool GameEntity::checkBallCollision(glm::vec3 ballPos, float ballRadius, glm::vec3 &hitNormal)
{
    if (!scene)
        return false;

    for (const auto &obj : scene->getGameObjects())
    {
        // Skip the ball itself
        if (obj.get() == scene->ball)
            continue;

        // Skip objects that are game entities (capsules, etc.)
        if (!obj->gameEntity.empty())
            continue;

        // Get world AABB and check collision
        AABB worldAABB = obj->GetWorldAABB();
        if (sphereAABBCollision(ballPos, ballRadius, worldAABB, hitNormal))
        {
            std::cout << "[Collision] Ball hit: " << obj->name << std::endl;
            return true;
        }
    }

    return false;
}

// =============================================================================
// ACTION BUFFER SYSTEM
// =============================================================================

bool GameEntity::bufferAction(const BufferedAction &action)
{
    if (action.type == ActionType::MOVE && hasBufferedMove())
    {
        std::cout << "[Buffer] Rejected: already has a MOVE action" << std::endl;
        return false;
    }
    if (action.type != ActionType::MOVE && hasBufferedNonMove())
    {
        std::cout << "[Buffer] Rejected: already has a non-MOVE action" << std::endl;
        return false;
    }
    actionBuffer.push_back(action);
    std::cout << "[Buffer] Added: " << action.description << std::endl;
    return true;
}

void GameEntity::removeAction(int index)
{
    if (index >= 0 && index < static_cast<int>(actionBuffer.size()))
    {
        std::cout << "[Buffer] Removed: " << actionBuffer[index].description << std::endl;
        actionBuffer.erase(actionBuffer.begin() + index);
    }
}

void GameEntity::clearActions()
{
    actionBuffer.clear();
}

bool GameEntity::hasBufferedMove() const
{
    for (const auto &a : actionBuffer)
    {
        if (a.type == ActionType::MOVE)
            return true;
    }
    return false;
}

bool GameEntity::hasBufferedNonMove() const
{
    for (const auto &a : actionBuffer)
    {
        if (a.type != ActionType::MOVE)
            return true;
    }
    return false;
}
