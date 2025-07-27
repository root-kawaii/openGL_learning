#pragma once
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include "model.h"

class Object {
    public:
        Object(Model& model, glm::vec3 position);
        ~Object();
        Model& model;
        glm::vec3 position;
    
    private:
        unsigned int VAO, VBO;
        unsigned int texture = 0;
        int vertexCount;
    };
    