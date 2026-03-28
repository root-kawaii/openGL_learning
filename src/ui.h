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
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../src/shader_m.h"
#include "input.h"
#include <ft2build.h>
#include FT_FREETYPE_H

// =============================================================================
// CONSTANTS
// =============================================================================
static const float REFERENCE_WIDTH = 1440.0f;
static const float REFERENCE_HEIGHT = 1440.0f;

// =============================================================================
// STRUCTS
// =============================================================================
struct Character
{
    unsigned int TextureID;
    glm::ivec2 Size;
    glm::ivec2 Bearing;
    unsigned int Advance;
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
    float x_pos; // Stored in SCREEN coordinates
    float y_pos; // Stored in SCREEN coordinates
    float r;
    float g;
    float b;
    float a;
    float scale; // Stored in SCREEN scale
    UIElementEnum elementType;
    bool pressed = false;
    std::function<void(std::string)> functionPtr;
};

// =============================================================================
// UI MANAGER CLASS
// =============================================================================
class Game;
class GameEntity;

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

    Shader uiShader;
    Shader textShader;

    // Setup
    void setupQuadGeometry();
    unsigned int setUpFont();

    // Coordinate scaling helpers (reference -> screen)
    float scaleX(float value) const
    {
        return value * (static_cast<float>(screenWidth) / REFERENCE_WIDTH);
    }

    float scaleY(float value) const
    {
        return value * (static_cast<float>(screenHeight) / REFERENCE_HEIGHT);
    }

    // Low-level rendering (works in SCREEN coordinates)
    void renderBoxScreen(float x, float y, float w, float h,
                         float r, float g, float b, float a);
    void renderTextScreen(const std::string &text, float x, float y,
                          float scale, glm::vec3 color);

public:
    unsigned int screenWidth;
    unsigned int screenHeight;
    bool isCharacterMoving = false;
    bool isPassing = false;
    std::shared_ptr<GameEntity> passTargetEntity = nullptr;

    Game* gameInstance = nullptr;

    UIManager(unsigned int height, unsigned int width);
    ~UIManager();

    // Callbacks
    void onMovePressed(std::string value);
    void onShootPressed(std::string value);
    void onPassPressed(std::string value);
    void onWaitPressed(std::string value);
    void onEndTurnPressed(std::string value);
    void onExecuteTurnPressed(std::string value);

    // Setters
    void setWindow(GLFWwindow *gameWindow) { window = gameWindow; }
    void setInputManager(InputManager *inputsManager) { inputManager = inputsManager; }
    void setGame(Game* game) { gameInstance = game; }

    // Input handling
    bool isPressed(UIElement element);
    bool executeUI(UIElement element);
    bool isMouseOver(float mouseX, float mouseY, float width, float height,
                     float x_pos, float y_pos);

    // =========================================================================
    // PUBLIC RENDERING API (accepts REFERENCE coordinates - 1440x1440)
    // =========================================================================
    void renderUIBBox(float width, float height, float x_pos, float y_pos);
    void renderUIBBox(float width, float height, float x_pos, float y_pos,
                      float r, float g, float b, float alpha = 1.0f);
    void RenderText(std::string text, float x, float y, float scale, glm::vec3 color);
    void setProjectionMatrix(const glm::mat4 &matrix);

    // =========================================================================
    // UI BUILDING & RENDERING
    // =========================================================================
    void buildGameMenu();
    void buildActionBufferUI();
    void buildBottomCenterMenu();
    void renderAllUIElements(float mouseX, float mouseY);
    void clearUIElements();
    const std::vector<UIElement>& getUIElements() const { return uiElements; }

    // Legacy standalone render functions
    void renderGameMenu();
    void renderMenuDecorations(float menuX, float menuY, float menuWidth, float menuHeight);
    void renderStatusBars();
    void renderPauseMenu();

    // =========================================================================
    // ELEMENT MANAGEMENT (accepts REFERENCE coordinates, stores SCREEN coords)
    // =========================================================================

    // Add box without callback (REFERENCE coordinates)
    void addBox(float width, float height, float x, float y,
                float r, float g, float b, float a)
    {
        UIElement element;
        element.width = scaleX(width);
        element.height = scaleY(height);
        element.x_pos = scaleX(x);
        element.y_pos = scaleY(y);
        element.r = r;
        element.g = g;
        element.b = b;
        element.a = a;
        element.elementType = BOX;
        element.functionPtr = nullptr;
        uiElements.push_back(element);
    }

    // Add box with callback (REFERENCE coordinates)
    void addBox(float width, float height, float x, float y,
                float r, float g, float b, float a,
                std::function<void(std::string)> functionPointer)
    {
        UIElement element;
        element.width = scaleX(width);
        element.height = scaleY(height);
        element.x_pos = scaleX(x);
        element.y_pos = scaleY(y);
        element.r = r;
        element.g = g;
        element.b = b;
        element.a = a;
        element.elementType = BOX;
        element.functionPtr = functionPointer;
        uiElements.push_back(element);
    }

    // Add text (REFERENCE coordinates)
    void addText(const std::string &text, float x, float y, float scale,
                 float r, float g, float b)
    {
        UIElement element;
        element.text = text;
        element.x_pos = scaleX(x);
        element.y_pos = scaleY(y);
        element.scale = scale * (static_cast<float>(screenWidth) / REFERENCE_WIDTH);
        element.r = r;
        element.g = g;
        element.b = b;
        element.a = 1.0f;
        element.elementType = TEXT;
        uiElements.push_back(element);
    }
};