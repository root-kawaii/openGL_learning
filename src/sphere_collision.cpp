#include "sphere_collision.h"

CollisionInfo SphereCollision::sphereVsSphereCollision(const GameObject &a, const GameObject &b)
{
    CollisionInfo info;
    glm::vec3 diff = b.position - a.position;

    float distanceSq = glm::dot(diff, diff); // squared distance
    float radiusSum = a.collisionRadius + b.collisionRadius;

    if (distanceSq < radiusSum * radiusSum)
    {
        float distance = std::sqrt(distanceSq); // only compute sqrt when necessary
        info.normal = diff / distance;          // normalized vector
        info.penetration = radiusSum - distance;
        info.contactPoint = a.position + info.normal * a.collisionRadius;
        info.isValid = true;
    }

    return info; // RVO/move makes this efficient
}

glm::vec3 SphereCollision::simplePositionCorrection(const GameObject &a, const GameObject &b)
{
    CollisionInfo collision = sphereVsSphereCollision(a, b);
    if (!collision.isValid)
        return glm::vec3(0, 0, 0);

    // ⚠️ remove console printing in release builds!
    // std::cout << "pushingggggggggggggggg" << std::endl;

    return collision.normal * collision.penetration;
}

glm::vec3 SphereCollision::cameraPositionCorrection(const Camera &a, const GameObject &b)
{
    CollisionInfo collision = cameraVsSphereCollision(a, b);
    if (!collision.isValid)
        return glm::vec3(0, 0, 0);

    // ⚠️ remove console printing in release builds!
    // std::cout << "pushingggggggggggggggg" << std::endl;

    return collision.normal * collision.penetration;
}

CollisionInfo SphereCollision::cameraVsSphereCollision(const Camera &a, const GameObject &b)
{
    float cameraCollisionRadius = 1.0f;
    CollisionInfo info;
    glm::vec3 diff = b.position - a.Position;

    float distanceSq = glm::dot(diff, diff);
    float radiusSum = cameraCollisionRadius + b.collisionRadius;

    if (distanceSq < radiusSum * radiusSum)
    {
        float distance = std::sqrt(distanceSq);

        // --- CRITICAL FIX: Handle the division by zero edge case ---
        if (distance == 0.0f)
        {
            // Objects are at the exact same position. Return a default normal.
            info.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            info.penetration = radiusSum; // Max penetration
        }
        else
        {
            info.normal = diff / distance;
            info.penetration = radiusSum - distance;
        }

        info.contactPoint = a.Position + info.normal * cameraCollisionRadius;
        info.isValid = true;
    }

    return info;
}
