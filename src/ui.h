#pragma once
// OpenGL headers - MUST be first
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../src/shader_m.h"
#include <ft2build.h>
#include FT_FREETYPE_H

struct Character
{
    unsigned int TextureID; // ID handle of the glyph texture
    glm::ivec2 Size;        // Size of glyph
    glm::ivec2 Bearing;     // Offset from baseline to left/top of glyph
    unsigned int Advance;   // Offset to advance to next glyph
};

class UIManager
{
private:
    // OpenGL objects
    unsigned int uiShaderProgram;
    unsigned int textShaderProgram;
    unsigned int textVAO, textVBO;
    unsigned int uiVAO, uiVBO, uiEBO;

    std::map<char, Character> characters;

    // Shader sources
    Shader uiShader;
    Shader textShader;

    unsigned int screenWidth;
    unsigned int screenHeight;

    void setupQuadGeometry();
    unsigned int setUpFont();

public:
    UIManager(unsigned int height, unsigned int width);
    ~UIManager();

    void renderUIBBox(float width, float height, float x_pos, float y_pos);
    void renderUIBBox(float width, float height, float x_pos, float y_pos,
                      float r, float g, float b, float alpha = 1.0f);

    // Utility functions
    void setProjectionMatrix(const glm::mat4 &matrix);

    void RenderText(std::string text, float x, float y, float scale, glm::vec3 color);

    void renderGameMenu();

    void renderMenuDecorations(float menuX, float menuY, float menuWidth, float menuHeight);

    void renderStatusBars();
};

;