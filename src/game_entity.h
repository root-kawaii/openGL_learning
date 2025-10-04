#pragma once

#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include "../src/game_object.h"

class GameEntity
{
private:
    float timeWaitMilliseconds = 300;
    float timeSinceMovement = 0;

public:
    GameEntity();
    GameEntity(std::string entityName, std::shared_ptr<GameObject> gameObject);

    std::shared_ptr<GameObject> object;
    glm::vec3 targetDestination;
    bool isMoving = false;

    void move(float deltaTime);
    bool isGroundHigher(glm::vec3 position);
    bool isGroundLower(glm::vec3 position);
};