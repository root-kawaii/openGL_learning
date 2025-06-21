#include <iostream>
#include "../src/shader_m.h"

class TextureDebugger {
private:
    unsigned int quadVAO, quadVBO;
    unsigned int debugShader;
    
    // Fullscreen quad vertices
    float quadVertices[24] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

public:
    TextureDebugger() {
        setupQuad();
        // Assume you have a function to load shaders
        Shader debugShader("shaders/debug.vs", "shaders/debug.fs");
    }
    
    void setupQuad() {
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }
    
    void visualizeTexture(unsigned int textureID, int mode = 0, float nearPlane = 0.1f, float farPlane = 100.0f) {
        glUseProgram(debugShader);
        
        // Set uniforms
        glUniform1i(glGetUniformLocation(debugShader, "debugTexture"), 0);
        glUniform1i(glGetUniformLocation(debugShader, "visualizationMode"), mode);
        glUniform1f(glGetUniformLocation(debugShader, "depthNear"), nearPlane);
        glUniform1f(glGetUniformLocation(debugShader, "depthFar"), farPlane);
        
        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        // Render fullscreen quad
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
    
    // Convenience functions for different G-buffer components
    void visualizeAlbedo(unsigned int gAlbedoSpec) {
        visualizeTexture(gAlbedoSpec, 0);
    }
    
    void visualizeNormals(unsigned int gNormal) {
        visualizeTexture(gNormal, 1);
    }
    
    void visualizePosition(unsigned int gPosition) {
        visualizeTexture(gPosition, 2);
    }
    
    void visualizeDepth(unsigned int gDepth, float near = 0.1f, float far = 100.0f) {
        visualizeTexture(gDepth, 3, near, far);
    }
};
