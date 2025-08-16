#pragma once
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include "model.h"
#include <filesystem>

class GameObject {
    public:
        GameObject(std::string name, std::string modelPath, glm::vec3 position, glm::vec3 rotaion, glm::vec3 scale);
        ~GameObject();

        void setRadius(float radius){collisionRadius = radius;};

        Model model;
        std::string modelPath;
        glm::vec3 position;
        glm::vec3 rotaion;
        glm::vec3 scale;
        float collisionRadius;
        float ID;
        std::string name; 
    
    private:
        unsigned int VAO, VBO;
        unsigned int texture = 0;
        int vertexCount;
    };
    