#pragma once
#include <glad/glad.h>
#include <vector>

class Object {
    public:
        Object(const float* vertices, size_t vertexSize, int stride, unsigned int textureID = 0);
        ~Object();
        void draw() const;
    
    private:
        unsigned int VAO, VBO;
        unsigned int texture = 0;
        int vertexCount;
    };
    