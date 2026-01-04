#pragma once

#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include "../src/game_object.h"
#include <cmath>

class GameEntity
{
private:
    float timeWaitMilliseconds = 300;
    float timeSinceMovement = 0;

    float speed = 5;

public:
    GameEntity();
    GameEntity(std::string entityName, std::shared_ptr<GameObject> gameObject);

    std::shared_ptr<GameObject> object;
    glm::vec3 targetDestination;
    bool isMoving = false;
    bool hasMovedThisTurn = false;

    void move(float deltaTime);
    bool isGroundHigher(glm::vec3 position);
    bool isGroundLower(glm::vec3 position);

    float distanceFromGameEntity(glm::vec3 position);

    bool isReachable(glm::vec3 position);
    void moveToTarget(GameEntity &targetEntity);
};