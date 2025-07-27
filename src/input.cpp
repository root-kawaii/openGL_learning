#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <../src/camera.h>
#include <../src/raycast.h>

#include <iostream>

#include <filesystem>

#include <random>
#include <chrono>
#include <cstdlib>
#include <ctime>


bool gameMode = false;
bool shadowsKeyPressed = false;

void processInput(GLFWwindow *window, Camera *camera, float deltaTime, bool &shadows, float &seed)
{
    if(!gameMode){
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera->ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)   
         camera->ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera->ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera->ProcessKeyboard(RIGHT, deltaTime);
    // if(glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS){
    //     for(int indexone = 0; indexone < 5; indexone++){
    //         if(indexone == 3) {return;}
    //         if(indexone == 4){ return;}
    //         std::cout <<  positions[indexone] << std::endl ;
    //         positions[indexone] += 0.001;
    //         std::cout <<  positions[indexone] << std::endl;
    //         glBindBuffer(GL_ARRAY_BUFFER, VBO);
    //         glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(positions), positions);
    //     }
    //     // std::cout << "HI";
    // }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !shadowsKeyPressed)
    {
        shadows =! shadows;
        shadowsKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE)
    {
        shadowsKeyPressed = false;
    }
    // if(glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS){
    //     for(int indexone = 0; indexone < 5; indexone++){
    //         if(indexone == 3) {return;}
    //         if(indexone == 4){ return;}
    //         std::cout <<  positions[indexone] << std::endl ;
    //         positions[indexone] -= 0.001;
    //         std::cout <<  positions[indexone] << std::endl;
    //         glBindBuffer(GL_ARRAY_BUFFER, VBO);
    //         glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(positions), positions);
    //     }
    //     // std::cout << "HI";
    // }
    if(glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS){
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        seed = dis(gen) * 1;
    }
    if(glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS){
        gameMode = true;
    }
    }else{
    if(glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS){
        gameMode = false;
}
if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
glfwSetWindowShouldClose(window, true);
}
}