#pragma once

#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include "model.h"
#include "game_object.h"
#include "camera.h"
#include <filesystem>

struct CollisionInfo
{
    int objectA, objectB;
    glm::vec3 contactPoint;
    glm::vec3 normal;
    float penetration;
    bool isValid = false;

    CollisionInfo() {}

    CollisionInfo(int a, int b, glm::vec3 point, glm::vec3 n, float pen)
        : objectA(a), objectB(b), contactPoint(point), normal(n), penetration(pen) {}
};

class SphereCollision
{
public:
    CollisionInfo sphereVsSphereCollision(const GameObject &a, const GameObject &b);
    glm::vec3 simplePositionCorrection(const GameObject &a, const GameObject &b);
    glm::vec3 cameraPositionCorrection(const Camera &a, const GameObject &b);
    CollisionInfo cameraVsSphereCollision(const Camera &a, const GameObject &b);

private:
};