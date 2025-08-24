#pragma once
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include "model.h"
#include <filesystem>

class GameObject
{
public:
    GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotaion, glm::vec3 scale);
    GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotaion, glm::vec3 scale, float collisionRadius);
    GameObject(std::shared_ptr<GameObject> gameObject);
    ~GameObject();

    void setRadius(float radius) { collisionRadius = radius; };

    Model model;
    std::string modelPath;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    glm::vec3 speed;
    glm::vec3 acceleration = glm::vec3(0, 0, 0);
    float collisionRadius = 0;
    float ID;
    std::string name;

private:
    unsigned int VAO, VBO;
    unsigned int texture = 0;
    int vertexCount;
};
