#pragma once
#include <glad/glad.h>
#include <vector>

class Object {
public:
    unsigned int VAO, VBO;
    size_t vertexCount;

    Object(const float* vertices, size_t vertexSize, int stride = 5);
    ~Object();

    void draw() const;
};
