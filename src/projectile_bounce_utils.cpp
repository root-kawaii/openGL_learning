#include "projectile_bounce_utils.h"

#include <glm/gtx/norm.hpp>

bool ProjectileBounceUtils::sphereIntersectsAabb(const glm::vec3 &sphereCenter,
                                                 float radius,
                                                 const AABB &box,
                                                 glm::vec3 &hitNormal)
{
    if (!box.IsValid())
        return false;

    glm::vec3 closestPoint;
    closestPoint.x = glm::clamp(sphereCenter.x, box.min.x, box.max.x);
    closestPoint.y = glm::clamp(sphereCenter.y, box.min.y, box.max.y);
    closestPoint.z = glm::clamp(sphereCenter.z, box.min.z, box.max.z);

    glm::vec3 delta = sphereCenter - closestPoint;
    float distanceSq = glm::length2(delta);
    float radiusSq = radius * radius;
    if (distanceSq > radiusSq)
        return false;

    if (distanceSq > 1e-8f)
    {
        hitNormal = glm::normalize(delta);
        return true;
    }

    glm::vec3 toCenter = sphereCenter - box.GetCenter();
    glm::vec3 halfSize = box.GetSize() * 0.5f;
    glm::vec3 penetration = halfSize - glm::abs(toCenter);

    if (penetration.x < penetration.y && penetration.x < penetration.z)
        hitNormal = glm::vec3(toCenter.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
    else if (penetration.y < penetration.z)
        hitNormal = glm::vec3(0.0f, toCenter.y > 0.0f ? 1.0f : -1.0f, 0.0f);
    else
        hitNormal = glm::vec3(0.0f, 0.0f, toCenter.z > 0.0f ? 1.0f : -1.0f);

    return true;
}

std::optional<ProjectileBounceHit> ProjectileBounceUtils::sweepSphereAgainstObjects(
    const glm::vec3 &start,
    const glm::vec3 &end,
    float radius,
    const std::vector<std::shared_ptr<GameObject>> &objects,
    const std::function<bool(const std::shared_ptr<GameObject> &)> &shouldCollide)
{
    glm::vec3 delta = end - start;
    bool foundHit = false;
    ProjectileBounceHit closestHit;
    closestHit.time = 1.0f;

    for (const auto &obj : objects)
    {
        if (!obj || (shouldCollide && !shouldCollide(obj)))
            continue;

        AABB aabb = obj->GetWorldAABB();
        if (!aabb.IsValid())
            continue;

        glm::vec3 expandedMin = aabb.min - glm::vec3(radius);
        glm::vec3 expandedMax = aabb.max + glm::vec3(radius);

        float tEnter = 0.0f;
        float tExit = 1.0f;
        glm::vec3 hitNormal(0.0f);
        bool hit = true;

        for (int axis = 0; axis < 3; ++axis)
        {
            float startAxis = start[axis];
            float deltaAxis = delta[axis];
            float minAxis = expandedMin[axis];
            float maxAxis = expandedMax[axis];

            if (std::abs(deltaAxis) < 1e-6f)
            {
                if (startAxis < minAxis || startAxis > maxAxis)
                {
                    hit = false;
                    break;
                }
                continue;
            }

            float invDelta = 1.0f / deltaAxis;
            float t1 = (minAxis - startAxis) * invDelta;
            float t2 = (maxAxis - startAxis) * invDelta;

            glm::vec3 nearNormal(0.0f);
            if (t1 <= t2)
                nearNormal[axis] = -1.0f;
            else
            {
                std::swap(t1, t2);
                nearNormal[axis] = 1.0f;
            }

            if (t1 > tEnter)
            {
                tEnter = t1;
                hitNormal = nearNormal;
            }

            tExit = std::min(tExit, t2);
            if (tEnter > tExit)
            {
                hit = false;
                break;
            }
        }

        if (!hit || tEnter < 0.0f || tEnter > 1.0f)
            continue;

        if (glm::length2(hitNormal) < 1e-8f)
        {
            glm::vec3 fallbackNormal;
            if (!sphereIntersectsAabb(start, radius, aabb, fallbackNormal))
                fallbackNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            hitNormal = fallbackNormal;
        }

        hitNormal = glm::normalize(hitNormal);

        // If we're already on the contact boundary and moving away from this
        // face, don't treat it as a fresh collision.
        if (tEnter <= 1e-4f && glm::dot(delta, hitNormal) >= 0.0f)
            continue;

        if (!foundHit || tEnter < closestHit.time)
        {
            foundHit = true;
            closestHit.time = tEnter;
            closestHit.point = start + delta * tEnter;
            closestHit.normal = hitNormal;
            closestHit.object = obj;
        }
    }

    if (!foundHit)
        return std::nullopt;

    return closestHit;
}

glm::vec3 ProjectileBounceUtils::computeBounceVelocity(const glm::vec3 &incomingVelocity,
                                                       const glm::vec3 &surfaceNormal,
                                                       const ProjectileBounceConfig &config)
{
    glm::vec3 normal = glm::normalize(surfaceNormal);
    glm::vec3 normalComponent = glm::dot(incomingVelocity, normal) * normal;
    glm::vec3 tangentialComponent = incomingVelocity - normalComponent;
    glm::vec3 bouncedVelocity =
        (-normalComponent * config.restitution) +
        (tangentialComponent * config.tangentialDamping);

    if (glm::length(bouncedVelocity) < config.minBounceSpeed)
        return glm::vec3(0.0f);

    return bouncedVelocity;
}

glm::vec3 ProjectileBounceUtils::resolveBouncePosition(const ProjectileBounceHit &hit,
                                                       const ProjectileBounceConfig &config)
{
    return hit.point + hit.normal * config.separationEpsilon;
}
