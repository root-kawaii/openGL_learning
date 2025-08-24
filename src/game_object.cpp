#include "game_object.h"

#include <iostream>

namespace fs = std::filesystem;

GameObject::GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale)
    : model(fs::path(modelPath)), // Initialize Model here!
      name(name),
      position(position),
      rotation(rotation),
      scale(scale),
      speed(glm::vec3(0, 0, 0))
{
}

GameObject::GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, float collisionRadius)
    : model(fs::path(modelPath)), // Initialize Model here!
      name(name),
      position(position),
      rotation(rotation),
      scale(scale),
      speed(glm::vec3(0, 0, 0)),
      collisionRadius(collisionRadius)
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
