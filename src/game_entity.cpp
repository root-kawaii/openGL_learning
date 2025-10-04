#include "game_entity.h"
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
            object->position += glm::vec3(x_step, 1, 0);
        else if (isGroundLower(next_pos))
            object->position += glm::vec3(x_step, -1, 0);
        else
            object->position += glm::vec3(x_step, 0, 0);
        timeSinceMovement = 0;
        return;
    }

    if (std::abs(z_distance) > epsilon)
    {
        float z_step = (z_distance > 0) ? 1.0f : -1.0f;
        glm::vec3 next_pos = object->position + glm::vec3(0, 0, z_step);

        if (isGroundHigher(next_pos))
            object->position += glm::vec3(0, 1, z_step);
        else if (isGroundLower(next_pos))
            object->position += glm::vec3(0, -1, z_step);
        else
            object->position += glm::vec3(0, 0, z_step);

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
