#include "game_object.h"

#include <iostream>

namespace fs = std::filesystem;

GameObject::GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale)
    : model(fs::path(modelPath)), // Initialize Model here!
      modelPath(modelPath),
      name(name),
      position(position),
      rotation(rotation),
      scale(scale),
      speed(glm::vec3(0, 0, 0))
{
}

GameObject::GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, float collisionRadius)
    : model(fs::path(modelPath)), // Initialize Model here!
      modelPath(modelPath),
      name(name),
      position(position),
      rotation(rotation),
      scale(scale),
      speed(glm::vec3(0, 0, 0)),
      collisionRadius(collisionRadius)
{
}

GameObject::GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, float collisionRadius, std::string shaderName)
    : model(fs::path(modelPath)), // Initialize Model here!
      modelPath(modelPath),
      name(name),
      position(position),
      rotation(rotation),
      scale(scale),
      speed(glm::vec3(0, 0, 0)),
      collisionRadius(collisionRadius),
      shaderName(shaderName)
{
}

GameObject::GameObject(std::shared_ptr<GameObject> gameObject)
    : model(gameObject->model) // Copy the model
      ,
      modelPath(gameObject->modelPath) // Copy model path
      ,
      position(gameObject->position) // Copy position
      ,
      rotation(gameObject->rotation) // Copy rotation (note: typo in original)
      ,
      scale(gameObject->scale) // Copy scale
      ,
      speed(gameObject->speed) // Copy speed
      ,
      collisionRadius(gameObject->collisionRadius) // Copy collision radius
      ,
      ID(gameObject->ID) // Copy ID
      ,
      name(gameObject->name) // Copy name
      ,
      VAO(gameObject->VAO) // Copy VAO
      ,
      VBO(gameObject->VBO) // Copy VBO
      ,
      texture(gameObject->texture) // Copy texture
      ,
      vertexCount(gameObject->vertexCount) // Copy vertex count
{
  // Note: This is a shallow copy - both objects will share the same OpenGL resources
  // This is usually fine for bullets since they share the same mesh/texture data
}

GameObject::~GameObject()
{
}

glm::mat4 GameObject::GetTransform() const
{
  // Create transformation matrix: T * R * S (Translate * Rotate * Scale)
  glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

  // Create rotation matrix from Euler angles (assuming rotation is in radians)
  // Order: Y * X * Z (Yaw * Pitch * Roll)
  glm::mat4 rotationMatrix = glm::eulerAngleYXZ(rotation.y, rotation.x, rotation.z);

  glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

  return translationMatrix * rotationMatrix * scaleMatrix;
}

void GameObject::SetTransform(const glm::mat4 &transform)
{
  // Decompose the matrix back into position, rotation, and scale
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::quat orientation;

  glm::decompose(transform, scale, orientation, position, skew, perspective);

  // Convert quaternion to Euler angles
  rotation = glm::eulerAngles(orientation);
}

void GameObject::GetTransformFloat16(float *matrix) const
{
  // Get the transform matrix and copy to float array
  glm::mat4 transform = GetTransform();
  memcpy(matrix, glm::value_ptr(transform), sizeof(float) * 16);
}

void GameObject::SetTransformFromFloat16(const float *matrix)
{
  // Create matrix from float array and set transform
  glm::mat4 transform = glm::make_mat4(matrix);
  SetTransform(transform);
}

// Add these methods to your GameObject.cpp file

void GameObject::CalculateAABB()
{
  // This method should be called after your model is loaded
  // You'll need to modify this based on how your Model class stores vertex data

  localAABB = AABB(); // Reset AABB

  const auto &vertices = model.GetAllVertices(); // You'll need to implement this in Model class
  for (const auto &vertex : vertices)
  {
    localAABB.ExpandToInclude(vertex.Position);
  }

  aabbCalculated = true;
}

AABB GameObject::GetLocalAABB() const
{
  if (!aabbCalculated)
  {
    // If AABB hasn't been calculated yet, calculate it now
    const_cast<GameObject *>(this)->CalculateAABB();
  }
  return localAABB;
}

AABB GameObject::GetWorldAABB() const
{
  AABB localBounds = GetLocalAABB();

  if (!localBounds.IsValid())
  {
    return AABB(); // Return invalid AABB
  }

  // Get the 8 corners of the local AABB
  glm::vec3 corners[8] = {
      glm::vec3(localBounds.min.x, localBounds.min.y, localBounds.min.z), // min corner
      glm::vec3(localBounds.max.x, localBounds.min.y, localBounds.min.z),
      glm::vec3(localBounds.min.x, localBounds.max.y, localBounds.min.z),
      glm::vec3(localBounds.max.x, localBounds.max.y, localBounds.min.z),
      glm::vec3(localBounds.min.x, localBounds.min.y, localBounds.max.z),
      glm::vec3(localBounds.max.x, localBounds.min.y, localBounds.max.z),
      glm::vec3(localBounds.min.x, localBounds.max.y, localBounds.max.z),
      glm::vec3(localBounds.max.x, localBounds.max.y, localBounds.max.z) // max corner
  };

  // Transform all corners to world space and find new min/max
  glm::mat4 worldTransform = GetTransform();

  AABB worldAABB;

  for (int i = 0; i < 8; ++i)
  {
    glm::vec4 worldCorner = worldTransform * glm::vec4(corners[i], 1.0f);
    glm::vec3 worldPos = glm::vec3(worldCorner);
    worldAABB.ExpandToInclude(worldPos);
  }

  return worldAABB;
}