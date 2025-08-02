#include "game_object.h"

#include <iostream>

namespace fs = std::filesystem;

GameObject::GameObject(std::string name, std::string modelPath, glm::vec3 position) 
        : model(fs::path(modelPath)),  // Initialize Model here!
          name(name),
          position(position)
{
}


GameObject::~GameObject() {

}
