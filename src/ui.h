#pragma once
// OpenGL headers - MUST be first
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../src/shader_m.h"
#include "input.h"
#include <ft2build.h>
#include FT_FREETYPE_H

struct Character
{
    unsigned int TextureID; // ID handle of the glyph texture
    glm::ivec2 Size;        // Size of glyph
    glm::ivec2 Bearing;     // Offset from baseline to left/top of glyph
    unsigned int Advance;   // Offset to advance to next glyph
};

enum UIElementEnum
{
    BOX,
    TEXT
};

struct UIElement
{
    std::string text;
    float width;
    float height;
    float x_pos;
    float y_pos;
    float r;
    float g;
    float b;
    float a;
    float scale;
    UIElementEnum elementType;
    bool pressed = false;
    std::function<void(std::string)> functionPtr; // Changed from function pointer to std::function
};

class UIManager
{
private:
    // OpenGL objects
    unsigned int uiShaderProgram;
    unsigned int textShaderProgram;
    unsigned int textVAO, textVBO;
    unsigned int uiVAO, uiVBO, uiEBO;

    GLFWwindow *window;
    InputManager *inputManager;

    std::map<char, Character> characters;
    std::vector<UIElement> uiElements;

    // Shader sources
    Shader uiShader;
    Shader textShader;

    void setupQuadGeometry();
    unsigned int setUpFont();

public:
    unsigned int screenWidth;
    unsigned int screenHeight;

    bool isCharacterMoving = false;

    UIManager(unsigned int height, unsigned int width);
    ~UIManager();

    // Callback member functions
    void onMovePressed(std::string value);
    void onActPressed(std::string value);
    void onWaitPressed(std::string value);
    void onStatusPressed(std::string value);
    void onAutoBattlePressed(std::string value);

    void setWindow(GLFWwindow *gameWindow) { window = gameWindow; };
    void setInputManager(InputManager *inputsManager) { inputManager = inputsManager; };

    bool isPressed(UIElement element);
    bool executeUI(UIElement element);

    void renderUIBBox(float width, float height, float x_pos, float y_pos);
    void renderUIBBox(float width, float height, float x_pos, float y_pos,
                      float r, float g, float b, float alpha = 1.0f);

    // Utility functions
    void setProjectionMatrix(const glm::mat4 &matrix);

    void RenderText(std::string text, float x, float y, float scale, glm::vec3 color);

    void renderGameMenu();
    void renderMenuDecorations(float menuX, float menuY, float menuWidth, float menuHeight);
    void renderStatusBars();
    void renderPauseMenu();

    void buildGameMenu();
    void buildBottomCenterMenu(); // Changed name from buildBottomCenterMenuWithCallbacks

    void renderAllUIElements(float mouseX, float mouseY);
    void clearUIElements();

    bool isMouseOver(float mouseX, float mouseY, float element_width, float element_height,
                     float element_x_pos, float element_y_pos);

    // addBox without callback
    void addBox(float width, float height, float x, float y,
                float r, float g, float b, float a)
    {
        UIElement element;
        element.width = width;
        element.height = height;
        element.x_pos = x;
        element.y_pos = y;
        element.r = r;
        element.g = g;
        element.b = b;
        element.a = a;
        element.elementType = BOX;
        element.functionPtr = nullptr;
        uiElements.push_back(element);
    }

    // addBox with callback - now accepts std::function (including lambdas)
    void addBox(float width, float height, float x, float y,
                float r, float g, float b, float a,
                std::function<void(std::string)> functionPointer)
    {
        UIElement element;
        element.width = width;
        element.height = height;
        element.x_pos = x;
        element.y_pos = y;
        element.r = r;
        element.g = g;
        element.b = b;
        element.a = a;
        element.elementType = BOX;
        element.functionPtr = functionPointer;
        uiElements.push_back(element);
    }

    void addText(const std::string &text, float x, float y, float scale,
                 float r, float g, float b)
    {
        UIElement element;
        element.text = text;
        element.x_pos = x;
        element.y_pos = y;
        element.scale = scale;
        element.r = r;
        element.g = g;
        element.b = b;
        element.a = 1.0f;
        element.elementType = TEXT;
        uiElements.push_back(element);
    }
};