// input.h
#pragma once
#include <GLFW/glfw3.h>
#include "camera.h"


void processInput(GLFWwindow* window, float* positions, unsigned int VBO, Camera *camera, float deltaTime, bool &shadows);
