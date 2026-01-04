#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>  // For distance2 (squared distance)
#include "game_object.h"
#include "camera.h"
#include "sphere_collision.h"

/**
 * Optimized collision detection system
 * Reduces O(N²) complexity with spatial partitioning and early-out checks
 */
class CollisionSystem
{
private:
    SphereCollision sphereCollision;

    // Performance tuning parameters
    float maxCollisionCheckDistance = 50.0f;  // Only check nearby objects

    /**
     * Quick distance-squared check (avoids expensive sqrt)
     * Returns true if objects might be colliding
     */
    inline bool mightCollide(const GameObject &a, const GameObject &b) const
    {
        // Early-out: Skip if either has no collision
        if (a.collisionRadius == 0.0f || b.collisionRadius == 0.0f)
            return false;

        // Quick bounding check using distance-squared
        float distSq = glm::distance2(a.position, b.position);
        float combinedRadius = a.collisionRadius + b.collisionRadius;
        float thresholdSq = combinedRadius * combinedRadius * 4.0f; // 2x safety margin

        return distSq < thresholdSq;
    }

public:
    CollisionSystem() = default;

    /**
     * Set maximum distance for collision checks
     * Objects farther than this won't be tested (performance optimization)
     */
    void setMaxCollisionDistance(float distance)
    {
        maxCollisionCheckDistance = distance;
    }

    /**
     * Perform camera-object collision detection
     * Returns correction vector for camera position
     */
    glm::vec3 checkCameraCollisions(Camera &camera,
                                     const std::vector<std::shared_ptr<GameObject>> &objects)
    {
        glm::vec3 totalCorrection = glm::vec3(0.0f);
        float maxDistanceSq = maxCollisionCheckDistance * maxCollisionCheckDistance;

        for (const auto &obj : objects)
        {
            if (obj->collisionRadius == 0.0f)
                continue;

            // Early-out: Only check if camera is reasonably close
            float cameraDistSq = glm::distance2(camera.Position, obj->position);
            if (cameraDistSq > maxDistanceSq)
                continue;

            totalCorrection -= sphereCollision.cameraPositionCorrection(camera, *obj);
        }

        return totalCorrection;
    }

    /**
     * Perform object-object collision detection and resolution
     * Uses O(N²) but with early-out optimizations
     *
     * TODO: Implement spatial partitioning (grid/octree) for O(N) average case
     */
    void checkObjectCollisions(std::vector<std::shared_ptr<GameObject>> &objects)
    {
        // Nested loop with early-out optimizations
        for (size_t m = 0; m < objects.size(); ++m)
        {
            auto &a = objects[m];
            if (a->collisionRadius == 0.0f)
                continue;

            for (size_t n = m + 1; n < objects.size(); ++n)
            {
                auto &b = objects[n];

                // Quick pre-check before expensive collision calculation
                if (!mightCollide(*a, *b))
                    continue;

                // Detailed collision check
                glm::vec3 correction = sphereCollision.simplePositionCorrection(*a, *b);

                if (glm::length(correction) > 0.0f)
                {
                    // Apply half the correction to each object for stability
                    a->position += correction * 0.5f;
                    b->position -= correction * 0.5f;
                }
            }
        }
    }

    /**
     * Combined collision pass - checks both camera and object collisions
     * Returns camera correction vector
     */
    glm::vec3 performCollisionPass(Camera &camera,
                                    std::vector<std::shared_ptr<GameObject>> &objects)
    {
        // 1. Object-object collisions (modifies object positions)
        checkObjectCollisions(objects);

        // 2. Camera-object collisions (returns correction for camera)
        return checkCameraCollisions(camera, objects);
    }
};
