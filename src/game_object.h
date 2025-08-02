#pragma once
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include "model.h"
#include <filesystem>

class GameObject {
    public:
        GameObject(std::string name, std::string modelPath, glm::vec3 position);
        ~GameObject();
        Model model;
        std::string modelPath;
        glm::vec3 position;
        float ID;
        std::string name; 
    
    private:
        unsigned int VAO, VBO;
        unsigned int texture = 0;
        int vertexCount;
    };
    