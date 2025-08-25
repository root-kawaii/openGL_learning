#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "game_object.h"

class ObjectPicker
{
private:
    struct Ray
    {
        glm::vec3 origin;
        glm::vec3 direction;
    };

public:
    // Convert screen coordinates to world ray
    Ray ScreenToWorldRay(float mouseX, float mouseY,
                         const glm::mat4 &view, const glm::mat4 &projection,
                         float screenWidth, float screenHeight)
    {
        // Normalize mouse coordinates to [-1, 1]
        float x = (2.0f * mouseX) / screenWidth - 1.0f;
        float y = 1.0f - (2.0f * mouseY) / screenHeight;

        // Create ray in clip space
        glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);

        // Transform to eye space
        glm::mat4 invProj = glm::inverse(projection);
        glm::vec4 rayEye = invProj * rayClip;
        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

        // Transform to world space
        glm::mat4 invView = glm::inverse(view);
        glm::vec4 rayWorld = invView * rayEye;

        Ray ray;
        ray.origin = glm::vec3(invView[3]); // Camera position
        ray.direction = glm::normalize(glm::vec3(rayWorld));

        return ray;
    }

    // Test ray-AABB intersection (for bounding boxes)
    bool RayAABBIntersect(const Ray &ray, const AABB &aabb, float &distance)
    {
        glm::vec3 invDir = 1.0f / ray.direction;
        glm::vec3 t1 = (aabb.min - ray.origin) * invDir;
        glm::vec3 t2 = (aabb.max - ray.origin) * invDir;

        glm::vec3 tmin = glm::min(t1, t2);
        glm::vec3 tmax = glm::max(t1, t2);

        float tNear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
        float tFar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

        if (tNear > tFar || tFar < 0.0f)
            return false;

        distance = tNear > 0.0f ? tNear : tFar;
        return true;
    }

    // Find closest object under mouse
    std::shared_ptr<GameObject> PickObject(float mouseX, float mouseY,
                                           const std::vector<shared_ptr<GameObject>> objects,
                                           const glm::mat4 &view, const glm::mat4 &projection,
                                           float screenWidth, float screenHeight)
    {
        Ray ray = ScreenToWorldRay(mouseX, mouseY, view, projection, screenWidth, screenHeight);

        std::shared_ptr<GameObject> closestObject = nullptr;
        float closestDistance = FLT_MAX;

        for (auto obj : objects)
        {
            AABB worldAABB = obj->GetWorldAABB(); // Transform AABB to world space
            float distance;

            if (RayAABBIntersect(ray, worldAABB, distance))
            {
                if (distance < closestDistance)
                {
                    closestDistance = distance;
                    closestObject = obj;
                }
            }
        }

        return closestObject;
    }
};