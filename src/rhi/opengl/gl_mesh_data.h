#pragma once

#include "../rhi_buffer.h"
#include "../../vertex.h"
#include <glad/glad.h>
#include <vector>
#include <cstddef>   // offsetof
#include <iostream>

// ─── GLMeshData ──────────────────────────────────────────────────────────────
//
// OpenGL implementation of RHIMeshBuffers.
// Wraps the classic VAO + VBO + EBO pattern that Mesh::setupMesh() used to
// do inline. Now the same GL code lives here, and Mesh holds a pointer to
// this through the RHIMeshBuffers interface.
//
// VAO (Vertex Array Object): stores the vertex attribute layout so we don't
//   need to re-specify it every draw call.
// VBO (Vertex Buffer Object): holds the actual vertex data on the GPU.
// EBO (Element Buffer Object): holds the index data for indexed drawing.

class GLMeshData : public RHIMeshBuffers {
public:
    unsigned int VAO = 0, VBO = 0, EBO = 0;

    GLMeshData() = default;

    // Upload vertex + index data and configure vertex attributes
    void setup(const std::vector<Vertex>& vertices,
               const std::vector<unsigned int>& indices)
    {
        indexCount_ = static_cast<uint32_t>(indices.size());

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     vertices.size() * sizeof(Vertex),
                     vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(unsigned int),
                     indices.data(), GL_STATIC_DRAW);

        // Vertex attribute layout — must match the Vertex struct
        // 0: Position  (vec3)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Position));
        // 1: Normal    (vec3)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Normal));
        // 2: TexCoords (vec2)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, TexCoords));
        // 3: Tangent   (vec3)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Tangent));
        // 4: Bitangent (vec3)
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Bitangent));
        // 5: BoneIDs   (ivec4) — note: integer attribute
        glEnableVertexAttribArray(5);
        glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex),
                               (void*)offsetof(Vertex, m_BoneIDs));
        // 6: Weights   (vec4)
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, m_Weights));

        glBindVertexArray(0);
    }

    // Bind VAO for drawing
    void bind()   const { glBindVertexArray(VAO); }
    void unbind() const { glBindVertexArray(0); }

    uint32_t getIndexCount() const override { return indexCount_; }

    ~GLMeshData() override {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);
    }

    // No copy, allow move
    GLMeshData(const GLMeshData&) = delete;
    GLMeshData& operator=(const GLMeshData&) = delete;
    GLMeshData(GLMeshData&& o) noexcept
        : VAO(o.VAO), VBO(o.VBO), EBO(o.EBO), indexCount_(o.indexCount_) {
        o.VAO = o.VBO = o.EBO = 0;
        o.indexCount_ = 0;
    }
    GLMeshData& operator=(GLMeshData&& o) noexcept {
        if (this != &o) {
            if (VAO) glDeleteVertexArrays(1, &VAO);
            if (VBO) glDeleteBuffers(1, &VBO);
            if (EBO) glDeleteBuffers(1, &EBO);
            VAO = o.VAO; VBO = o.VBO; EBO = o.EBO; indexCount_ = o.indexCount_;
            o.VAO = o.VBO = o.EBO = 0; o.indexCount_ = 0;
        }
        return *this;
    }

private:
    uint32_t indexCount_ = 0;
};
