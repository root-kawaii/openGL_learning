// input.h
#pragma once
#include <GLFW/glfw3.h>
#include "camera.h"


void processInput(GLFWwindow* window, Camera *camera, float deltaTime, bool &shadows);
