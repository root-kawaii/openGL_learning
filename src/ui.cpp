#include "ui.h"

UIManager::UIManager() : ciao(0)
{
    // Initialize OpenGL objects
    shaderProgram = createShaderProgram();
    setupQuadGeometry();

    // Set default orthographic projection (assuming 800x600 window)
    setProjectionMatrix(0.0f, 800.0f, 600.0f, 0.0f);
}

UIManager::~UIManager()
{
    // Clean up OpenGL resources
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);
}

unsigned int UIManager::compileShader(unsigned int type, const char *source)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // Check for compilation errors
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    }

    return shader;
}

unsigned int UIManager::createShaderProgram()
{
    unsigned int vertexShader =
        compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader =
        compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    // Clean up individual shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void UIManager::setupQuadGeometry()
{
    // Quad vertices (normalized coordinates from 0 to 1)
    float vertices[] = {
        // positions
        0.0f, 1.0f, // top left
        1.0f, 1.0f, // top right
        1.0f, 0.0f, // bottom right
        0.0f, 0.0f  // bottom left
    };

    unsigned int indices[] = {
        0, 1, 2, // first triangle
        2, 3, 0  // second triangle
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void UIManager::renderUIBBox(float width, float height, float x_pos,
                             float y_pos)
{
    // Default white color
    renderUIBBox(width, height, x_pos, y_pos, 1.0f, 1.0f, 1.0f, 1.0f);
}

void UIManager::renderUIBBox(float width, float height, float x_pos,
                             float y_pos, float r, float g, float b,
                             float alpha)
{
    glUseProgram(shaderProgram);

    // Set uniforms
    int positionLoc = glGetUniformLocation(shaderProgram, "position");
    int sizeLoc = glGetUniformLocation(shaderProgram, "size");
    int colorLoc = glGetUniformLocation(shaderProgram, "color");
    int alphaLoc = glGetUniformLocation(shaderProgram, "alpha");

    glUniform2f(positionLoc, x_pos, y_pos);
    glUniform2f(sizeLoc, width, height);
    glUniform3f(colorLoc, r, g, b);
    glUniform1f(alphaLoc, alpha);

    // Enable blending for alpha
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render the quad
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
}

void UIManager::setProjectionMatrix(float left, float right, float bottom,
                                    float top)
{
    glUseProgram(shaderProgram);

    // Create orthographic projection matrix
    float projection[16] = {2.0f / (right - left),
                            2.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            2.0f / (top - bottom),
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            -1.0f,
                            0.0f,
                            -(right + left) / (right - left),
                            -(top + bottom) / (top - bottom),
                            0.0f,
                            1.0f};

    int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, projection);
}