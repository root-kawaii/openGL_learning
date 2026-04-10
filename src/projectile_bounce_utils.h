#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "game_object.h"

struct ProjectileBounceHit
{
    glm::vec3 point = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    float time = 1.0f;
    std::shared_ptr<GameObject> object;
};

struct ProjectileBounceConfig
{
    float restitution = 0.62f;
    float tangentialDamping = 0.92f;
    float minBounceSpeed = 6.0f;
    float separationEpsilon = 0.12f;
};

class ProjectileBounceUtils
{
public:
    static bool sphereIntersectsAabb(const glm::vec3 &sphereCenter,
                                     float radius,
                                     const AABB &box,
                                     glm::vec3 &hitNormal);

    static std::optional<ProjectileBounceHit> sweepSphereAgainstObjects(
        const glm::vec3 &start,
        const glm::vec3 &end,
        float radius,
        const std::vector<std::shared_ptr<GameObject>> &objects,
        const std::function<bool(const std::shared_ptr<GameObject> &)> &shouldCollide);

    static glm::vec3 computeBounceVelocity(const glm::vec3 &incomingVelocity,
                                           const glm::vec3 &surfaceNormal,
                                           const ProjectileBounceConfig &config);

    static glm::vec3 resolveBouncePosition(const ProjectileBounceHit &hit,
                                           const ProjectileBounceConfig &config);
};
