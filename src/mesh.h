#pragma once

#include <glad/glad.h> // holds all OpenGL type declarations

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader_m.h"

#include "globals.h"
#include "vertex.h"
#include "rhi/opengl/gl_mesh_data.h"

#include <string>
#include <vector>
#include <memory>
using namespace std;

struct Mesh_Texture
{
    unsigned int id;
    string type;
    string path;
};

struct BoneInfo
{
    glm::mat4 OffsetMatrix;
    glm::mat4 FinalTransformation;

    BoneInfo(const glm::mat4 &offset)
    {
        OffsetMatrix = offset;
        FinalTransformation = glm::mat4(0.0f);
    }
};

class Mesh
{
public:
    // mesh Data — kept public so Vulkan backend can read vertex/index data
    vector<Vertex> vertices;
    vector<unsigned int> indices;
    vector<Mesh_Texture> textures;
    vector<BoneInfo> boneInfo;

    // GL buffer data (VAO/VBO/EBO) — accessed via RHI abstraction
    GLMeshData glData;

    // constructor
    Mesh(vector<Vertex> vertices, vector<unsigned int> indices, vector<Mesh_Texture> textures)
    {
        this->vertices = vertices;
        this->indices = indices;
        this->textures = textures;

        // Create OpenGL buffers through the RHI backend
        setupMesh();
    }

    // render the mesh (OpenGL path — Vulkan draw is handled by VulkanRenderer)
    void Draw(Shader &shader)
    {
        // bind appropriate textures
        unsigned int diffuseNr = 1;
        unsigned int specularNr = 1;
        unsigned int normalNr = 1;
        unsigned int heightNr = 1;
        unsigned int metallicNr = 1;
        unsigned int roughnessNr = 1;

        for (unsigned int i = 0; i < textures.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            string number;
            string name = textures[i].type;
            if (name == "texture_diffuse")
                number = std::to_string(diffuseNr++);
            else if (name == "texture_specular")
                number = std::to_string(specularNr++);
            else if (name == "texture_normal")
                number = std::to_string(normalNr++);
            else if (name == "texture_height")
                number = std::to_string(heightNr++);
            else if (name == "texture_metallic")
                number = std::to_string(metallicNr++);
            else if (name == "texture_roughness")
                number = std::to_string(roughnessNr++);

            glUniform1i(shader.getUniformLocation(name + number), i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        // Draw using GLMeshData
        glData.bind();
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glData.unbind();
        verticesDrawn += indices.size();
        trianglesDrawn += indices.size() / 3;
        drawCalls++;

        glActiveTexture(GL_TEXTURE0);
    }

private:
    // Initialize GPU buffers through the OpenGL RHI backend
    void setupMesh()
    {
        glData.setup(vertices, indices);
    }
};