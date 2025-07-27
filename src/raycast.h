#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "camera.h"
#include "mesh.h"

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

// Convert screen coordinates to a world-space ray
Ray screenToWorldRay(glm::vec2 screenPos, Camera& camera, float screenWidth, float screenHeight, glm::mat4 projectionMatrix);

// Check ray-triangle intersection
bool rayIntersectTriangle(const Ray& ray, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float& distance);

// Check ray against a full mesh
bool rayIntersectMesh(const Ray& ray, std::vector<Mesh> meshes);
