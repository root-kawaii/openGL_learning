#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <../src/camera.h>

#include <iostream>

#include <filesystem>


bool gameMode = false;

void processInput(GLFWwindow *window, float *positions, unsigned int VBO, Camera *camera, float deltaTime)
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
    if(glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS){
        for(int indexone = 0; indexone < 5; indexone++){
            if(indexone == 3) {return;}
            if(indexone == 4){ return;}
            std::cout <<  positions[indexone] << std::endl ;
            positions[indexone] += 0.001;
            std::cout <<  positions[indexone] << std::endl;
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(positions), positions);
        }
        // std::cout << "HI";
    }
    if(glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS){
        for(int indexone = 0; indexone < 5; indexone++){
            if(indexone == 3) {return;}
            if(indexone == 4){ return;}
            std::cout <<  positions[indexone] << std::endl ;
            positions[indexone] -= 0.001;
            std::cout <<  positions[indexone] << std::endl;
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(positions), positions);
        }
        // std::cout << "HI";
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