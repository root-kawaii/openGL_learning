#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <../src/shader_m.h>
#include <../src/camera.h>
#include <../src/model.h>

#include <../src/stb_image.h>
#include <../src/input.h>

#include <../src/game_object.h>
#include <../src/scene.h>

#include <iostream>
#include <../json/single_include/nlohmann/json.hpp>
#include <filesystem>

#include <chrono>
#include <thread>

#include <../src/raycast.h>

#include "../src/render_manager.h"


class Game {
private:
    // Core systems
    // EntityManager entityManager;
    RenderManager renderManager;
    // InputManager inputManager;
    // AudioManager audioManager;
    // SceneManager sceneManager;
    // PhysicsManager physicsManager;
    // ResourceManager resourceManager;
    
    // Game state
    // GameState currentState;
    float deltaTime;
    bool isRunning;
    bool isDebug;
    bool isGameMode;
    
    // Window/context
    GLFWwindow* window;
    
public:
    bool initialize();
    void run();
    void update(float deltaTime);
    void render();
    void cleanup();

    Game();
    ~Game();
    
    // System accessors
    // EntityManager& getEntityManager() { return entityManager; }
    RenderManager& getRenderManager() { return renderManager; }
    GLFWwindow* getWindow() { return window;}

    unsigned int SCR_WIDTH = 1400;
    unsigned int SCR_HEIGHT = 900;

    bool firstMouse = true;

    float lastX = SCR_WIDTH / 2.0f;
    float lastY = SCR_HEIGHT / 2.0f;

    float seed = rand();

    float xpos=0;
    float ypos=0;

    Camera camera;
    // InputManager& getInputManager() { return inputManager; }
    // SceneManager& getSceneManager() { return sceneManager; }
    // ... other getters

    
    bool initWindow() {
        camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 8); // Request 8x MSAA

    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

        // glfw window creation
        // --------------------
        window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Dreaming...", NULL, NULL);
        if (window == NULL)
        {
            std::cout << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }
        glfwMakeContextCurrent(window);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);

        // tell GLFW to capture our mouse
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // glad: load all OpenGL function pointers
        // ---------------------------------------
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cout << "Failed to initialize GLAD" << std::endl;
            return -1;
        }
        // TODO: Initialize GLFW, create window, setup OpenGL context
        // TODO: Initialize all managers (render, input, audio, etc.)
        // TODO: Load initial resources
        // TODO: Setup initial game state
        return true;
    }
    
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (game) game->framebuffer_size_callback_impl(width, height);
    }
    
    static void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (game) game->mouse_callback_impl(xposIn, yposIn);
    }
    
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (game) game->scroll_callback_impl(xoffset, yoffset);
    }
    
    void framebuffer_size_callback_impl(int width, int height) {
        glViewport(0, 0, width, height);
    }
    
    void mouse_callback_impl(double xposIn, double yposIn) {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);
        
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }
        
        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;
        
        camera.ProcessMouseMovement(xoffset, yoffset);
    }
    
    void scroll_callback_impl(double xoffset, double yoffset) {
        camera.ProcessMouseScroll(static_cast<float>(yoffset));
    }
};