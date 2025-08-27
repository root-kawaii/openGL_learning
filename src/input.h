// input.h
#pragma once
#include <GLFW/glfw3.h>
#include "camera.h"

class Game;

class InputManager
{
private:
public:
    void processInput(Game game, GLFWwindow *window, Camera *camera, float deltaTime, bool &shadows, float &seed);
};
