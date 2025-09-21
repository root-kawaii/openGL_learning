#pragma once
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "model.h"
#include <filesystem>
#include <glm/gtx/matrix_decompose.hpp>
#include <float.h>
#include "shader_m.h"

// AABB (Axis-Aligned Bounding Box) structure
struct AABB
{
    glm::vec3 min;
    glm::vec3 max;

    AABB() : min(FLT_MAX), max(-FLT_MAX) {}
    AABB(const glm::vec3 &minimum, const glm::vec3 &maximum)
        : min(minimum), max(maximum) {}

    // Check if AABB is valid
    bool IsValid() const
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    // Get center point
    glm::vec3 GetCenter() const
    {
        return (min + max) * 0.5f;
    }

    // Get size/dimensions
    glm::vec3 GetSize() const
    {
        return max - min;
    }

    // Expand AABB to include a point
    void ExpandToInclude(const glm::vec3 &point)
    {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    // Expand AABB to include another AABB
    void ExpandToInclude(const AABB &other)
    {
        if (other.IsValid())
        {
            min = glm::min(min, other.min);
            max = glm::max(max, other.max);
        }
    }
};

class GameObject
{
public:
    GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotaion, glm::vec3 scale);
    GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotaion, glm::vec3 scale, float collisionRadius);
    GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotaion, glm::vec3 scale, float collisionRadius, std::string shaderName);
    GameObject(std::shared_ptr<GameObject> gameObject);
    ~GameObject();

    void setRadius(float radius) { collisionRadius = radius; };

    // Transform methods for ImGuizmo integration
    glm::mat4 GetTransform() const;
    void SetTransform(const glm::mat4 &transform);

    // Helper method to get transform matrix as float array for ImGuizmo
    void GetTransformFloat16(float *matrix) const;
    void SetTransformFromFloat16(const float *matrix);

    // AABB methods
    AABB GetLocalAABB() const;
    AABB GetWorldAABB() const;
    void CalculateAABB(); // Call this after loading model to calculate local AABB

    Model model;
    std::string shaderName;
    std::string modelPath;
    glm::vec3 position;
    glm::vec3 rotation; // Euler angles in radians
    glm::vec3 scale;
    glm::vec3 speed;
    glm::vec3 acceleration = glm::vec3(0, 0, 0);
    float collisionRadius = 0;
    float ID;
    std::string name;

    glm::mat4 getModelMatrix() const
    {
        glm::mat4 modelMatrix = glm::mat4(1.0f);

        // 1. Apply translation
        modelMatrix = glm::translate(modelMatrix, position);

        // 2. Apply rotation
        glm::mat4 rotationMatrix = glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);
        modelMatrix *= rotationMatrix;

        // 3. Apply scale
        modelMatrix = glm::scale(modelMatrix, scale);

        return modelMatrix;
    }

private:
    unsigned int VAO, VBO;
    unsigned int texture = 0;
    int vertexCount;

    // Cached local AABB (calculated once after model loading)
    mutable AABB localAABB;
    mutable bool aabbCalculated = false;
};