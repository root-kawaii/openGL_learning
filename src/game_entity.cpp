#include "game_entity.h"
#include "scene.h"
#include <thread>
#include <chrono>

GameEntity::GameEntity()
{
}

GameEntity::GameEntity(std::string entityName, std::shared_ptr<GameObject> gameObject)
{
    object = gameObject;
    object->gameEntity = entityName;
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
        object->position = glm::vec3(newX, groundY, newZ);

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
        object->position = glm::vec3(newX, groundY, newZ);

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
    if (distanceFromGameEntity(position) <= speed)
        return true;
    return false;
}

void GameEntity::moveToTarget(GameEntity &targetEntity)
{
    targetDestination = targetEntity.object->position;
    isMoving = true;
}