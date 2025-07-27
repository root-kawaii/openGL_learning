#include "raycast.h"
#include <cfloat>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Convert screen coordinates to world ray
Ray screenToWorldRay(glm::vec2 screenPos, Camera& camera, float screenWidth, float screenHeight, glm::mat4 projectionMatrix) {
    float x = (2.0f * screenPos.x) / screenWidth - 1.0f;
    float y = 1.0f - (2.0f * screenPos.y) / screenHeight;

    glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 rayEye = glm::inverse(projectionMatrix) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    glm::vec4 rayWorld = glm::inverse(camera.GetViewMatrix()) * rayEye;
    glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld));

    std::cout << rayDir.x << std::endl;
        std::cout << rayDir.y << std::endl;
            std::cout << rayDir.z << std::endl;

    return Ray{camera.Position, rayDir};
}

bool rayIntersectTriangle(const Ray& ray, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float& distance) {
    const float EPSILON = 1e-7f;

    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);

    if (fabs(a) < EPSILON) return false;

    float f = 1.0f / a;
    glm::vec3 s = ray.origin - v0;
    float u = f * glm::dot(s, h);

    if (u < 0.0f || u > 1.0f) return false;

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray.direction, q);

    if (v < 0.0f || u + v > 1.0f) return false;

    distance = f * glm::dot(edge2, q);
    return distance > EPSILON;
}

bool rayIntersectMesh(const Ray& ray, std::vector<Mesh> meshes) {
    bool hit = false;
    float closestDistance = FLT_MAX;

    for (const Mesh& mesh : meshes) {
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            glm::vec3 v0 = mesh.vertices[mesh.indices[i]].Position;
            glm::vec3 v1 = mesh.vertices[mesh.indices[i + 1]].Position;
            glm::vec3 v2 = mesh.vertices[mesh.indices[i + 2]].Position;

            float distance;
            if (rayIntersectTriangle(ray, v0, v1, v2, distance)) {
                if (distance < closestDistance) {
                    closestDistance = distance;
                    hit = true;
                }
            }
        }
    }

    return hit;
}
