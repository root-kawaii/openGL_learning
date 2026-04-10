#include "game_object.h"

#include <iostream>

namespace fs = std::filesystem;

GameObject::GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale)
    : model(std::make_shared<Model>(fs::path(modelPath))),
      modelPath(modelPath),
      name(name),
      position(position),
      rotation(rotation),
      scale(scale),
      speed(glm::vec3(0, 0, 0))
{
}

GameObject::GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, float collisionRadius)
    : model(std::make_shared<Model>(fs::path(modelPath))),
      modelPath(modelPath),
      name(name),
      position(position),
      rotation(rotation),
      scale(scale),
      speed(glm::vec3(0, 0, 0)),
      collisionRadius(collisionRadius)
{
}

GameObject::GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, float collisionRadius, std::string shaderName, glm::vec3 color)
    : model(std::make_shared<Model>(fs::path(modelPath))),
      modelPath(modelPath),
      name(name),
      position(position),
      rotation(rotation),
      scale(scale),
      speed(glm::vec3(0, 0, 0)),
      collisionRadius(collisionRadius),
      shaderName(shaderName),
      color(color)
{
}

// Used by Scene when models are pre-loaded and cached (avoids redundant loads).
GameObject::GameObject(std::string name, std::shared_ptr<Model> modelPtr, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, float collisionRadius, std::string shaderName, glm::vec3 color)
    : model(modelPtr),
      name(name),
      position(position),
      rotation(rotation),
      scale(scale),
      speed(glm::vec3(0, 0, 0)),
      collisionRadius(collisionRadius),
      shaderName(shaderName),
      color(color)
{
}

GameObject::GameObject(std::shared_ptr<GameObject> gameObject)
    : model(gameObject->model) // Share the same Model (VAO/VBOs, no reload)
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
      gameEntity(gameObject->gameEntity)
      ,
      gameplayType(gameObject->gameplayType)
      ,
      keyId(gameObject->keyId)
      ,
      requiresKeyId(gameObject->requiresKeyId)
      ,
      lootItems(gameObject->lootItems)
      ,
      VAO(gameObject->VAO) // Copy VAO
      ,
      VBO(gameObject->VBO) // Copy VBO
      ,
      texture(gameObject->texture) // Copy texture
      ,
      vertexCount(gameObject->vertexCount), // Copy vertex count
      transformOverride(gameObject->transformOverride),
      useTransformOverride(gameObject->useTransformOverride)
{
  // Note: This is a shallow copy - both objects will share the same OpenGL resources
  // This is usually fine for bullets since they share the same mesh/texture data
}

GameObject::~GameObject()
{
}

glm::mat4 GameObject::GetTransform() const
{
  if (useTransformOverride)
  {
    return transformOverride;
  }

  // Create transformation matrix: T * R * S
  glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

  // Use XYZ Euler angles (most compatible with ImGuizmo)
  glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1, 0, 0));
  glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0, 1, 0));
  glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0, 0, 1));
  glm::mat4 rotationMatrix = rotationZ * rotationY * rotationX; // ZYX order for XYZ Euler

  glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

  return translationMatrix * rotationMatrix * scaleMatrix;
}

void GameObject::SetTransform(const glm::mat4 &transform)
{
  useTransformOverride = false;
  transformOverride = glm::mat4(1.0f);

  // Extract translation (last column)
  position = glm::vec3(transform[3]);

  // Extract scale (length of first three columns)
  scale.x = glm::length(glm::vec3(transform[0]));
  scale.y = glm::length(glm::vec3(transform[1]));
  scale.z = glm::length(glm::vec3(transform[2]));

  // Handle negative scales
  if (glm::determinant(transform) < 0)
  {
    scale.x = -scale.x;
  }

  // Remove scaling from rotation matrix
  glm::mat3 rotMatrix = glm::mat3(transform);
  rotMatrix[0] /= scale.x;
  rotMatrix[1] /= scale.y;
  rotMatrix[2] /= scale.z;

  // Extract Euler angles from rotation matrix (XYZ order)
  rotation.x = atan2(-rotMatrix[1][2], rotMatrix[2][2]);
  rotation.y = atan2(rotMatrix[0][2], sqrt(rotMatrix[1][2] * rotMatrix[1][2] + rotMatrix[2][2] * rotMatrix[2][2]));
  rotation.z = atan2(-rotMatrix[0][1], rotMatrix[0][0]);
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

  const auto &vertices = model->GetAllVertices();
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
