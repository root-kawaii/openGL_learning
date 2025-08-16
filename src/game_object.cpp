#include "game_object.h"

#include <iostream>

namespace fs = std::filesystem;

GameObject::GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotaion, glm::vec3 scale) 
        : model(fs::path(modelPath)),  // Initialize Model here!
          name(name),
          position(position),
          rotaion(rotaion),
          scale(scale)
{
}


GameObject::~GameObject() {

}
