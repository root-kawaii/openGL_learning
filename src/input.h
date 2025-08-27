// input.h
#pragma once
#include <GLFW/glfw3.h>
#include "camera.h"
#include <unordered_map>

class Game;

class InputManager
{
private:
    std::unordered_map<int, bool> previousKeyStates;

public:
    void processInput(Game *game, GLFWwindow *window, Camera *camera, float deltaTime, bool &shadows, float &seed);
    bool wasKeyJustPressed(int key, GLFWwindow *window);
};
