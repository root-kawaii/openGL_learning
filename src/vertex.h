#pragma once

#include <glm/glm.hpp>

// ─── Vertex ──────────────────────────────────────────────────────────────────
//
// Shared vertex format used by both OpenGL and Vulkan backends.
// Extracted from mesh.h so that RHI backend headers can include it
// without pulling in GL-specific mesh code.
//
// Total size: 152 bytes per vertex
//   Position   (vec3)  = 12 bytes
//   Normal     (vec3)  = 12 bytes
//   TexCoords  (vec2)  =  8 bytes
//   Tangent    (vec3)  = 12 bytes
//   Bitangent  (vec3)  = 12 bytes
//   BoneIDs    (4 int) = 16 bytes
//   Weights    (4 flt) = 16 bytes
//   Padding           = varies by compiler

#define MAX_BONE_INFLUENCE 4

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
    int m_BoneIDs[MAX_BONE_INFLUENCE];
    float m_Weights[MAX_BONE_INFLUENCE];

    Vertex() : Position(0.0f), Normal(0.0f), TexCoords(0.0f),
               Tangent(0.0f), Bitangent(0.0f)
    {
        for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
        {
            m_BoneIDs[i] = 0;
            m_Weights[i] = 0.0f;
        }
    }
};
