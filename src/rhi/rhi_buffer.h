#pragma once

#include <cstdint>

// ─── RHIMeshBuffers ──────────────────────────────────────────────────────────
//
// Abstract base for GPU-resident mesh data (vertex + index buffers).
//
// In OpenGL, this wraps a VAO + VBO + EBO.
// In Vulkan, this wraps VkBuffers (vertex + index) allocated via VMA.
//
// The key insight: both APIs need the same DATA (vertices + indices) uploaded
// to the GPU, but the mechanism is completely different:
//   - OpenGL: glGenBuffers, glBufferData, glVertexAttribPointer
//   - Vulkan: VMA allocation, staging buffer copy, layout transitions
//
// By abstracting this, Mesh::setupMesh() can create GPU buffers without
// knowing which API is active. The backend-specific draw code then accesses
// the concrete type (GLMeshData or VkMeshData) to issue draw calls.

class RHIMeshBuffers {
public:
    virtual ~RHIMeshBuffers() = default;
    virtual uint32_t getIndexCount() const = 0;
};
