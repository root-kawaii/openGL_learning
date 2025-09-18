#pragma once
// OpenGL headers - MUST be first
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

class UIManager
{
private:
    unsigned int ciao;

    // OpenGL objects
    unsigned int shaderProgram;
    unsigned int VAO, VBO, EBO;

    // Shader sources
    const char *vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        
        uniform mat4 projection;
        uniform vec2 position;
        uniform vec2 size;
        
        void main() {
            vec2 scaledPos = aPos * size + position;
            gl_Position = projection * vec4(scaledPos, 0.0, 1.0);
        }
    )";

    const char *fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        
        uniform vec3 color;
        uniform float alpha;
        
        void main() {
            FragColor = vec4(color, alpha);
        }
    )";

    // Helper functions
    unsigned int compileShader(unsigned int type, const char *source);
    unsigned int createShaderProgram();
    void setupQuadGeometry();

public:
    UIManager();
    ~UIManager();

    void renderUIBBox(float width, float height, float x_pos, float y_pos);
    void renderUIBBox(float width, float height, float x_pos, float y_pos,
                      float r, float g, float b, float alpha = 1.0f);

    // Utility functions
    void setProjectionMatrix(float left, float right, float bottom, float top);
};
;