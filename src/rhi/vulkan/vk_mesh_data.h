#pragma once

#include "../rhi_buffer.h"
#include "../../vertex.h"
#include "../../vulkan/vk_buffer.h"
#include <vulkan/vulkan.h>
#include <vector>

// ─── VkMeshData ──────────────────────────────────────────────────────────────
//
// Vulkan implementation of RHIMeshBuffers.
// Wraps two VMA-allocated buffers: one for vertices, one for indices.
//
// Unlike OpenGL (where VAO stores the attribute layout), Vulkan vertex
// attribute descriptions are part of the graphics PIPELINE, not the buffer.
// So VkMeshData just holds the raw buffer handles — the pipeline knows how
// to interpret the vertex data based on its VkVertexInputAttributeDescription.
//
// Buffers are uploaded via staging (CPU-visible → GPU-local copy) for
// optimal GPU read performance, same pattern as Phase 3-4.

class VkMeshData : public RHIMeshBuffers {
public:
    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};

    VkMeshData() = default;

    // Upload mesh data to GPU-local buffers via staging
    void setup(VmaAllocator allocator, VkDevice device,
               VkCommandPool commandPool, VkQueue queue,
               const std::vector<Vertex>& vertices,
               const std::vector<unsigned int>& indices)
    {
        allocator_ = allocator;
        indexCount_ = static_cast<uint32_t>(indices.size());

        // Upload vertices via staging buffer
        vertexBuffer = createBufferWithStaging(
            allocator, device, commandPool, queue,
            vertices.data(),
            vertices.size() * sizeof(Vertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        // Upload indices via staging buffer
        indexBuffer = createBufferWithStaging(
            allocator, device, commandPool, queue,
            indices.data(),
            indices.size() * sizeof(unsigned int),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }

    uint32_t getIndexCount() const override { return indexCount_; }

    ~VkMeshData() override {
        if (allocator_) {
            destroyBuffer(allocator_, vertexBuffer);
            destroyBuffer(allocator_, indexBuffer);
        }
    }

    // No copy, allow move
    VkMeshData(const VkMeshData&) = delete;
    VkMeshData& operator=(const VkMeshData&) = delete;
    VkMeshData(VkMeshData&& o) noexcept
        : vertexBuffer(o.vertexBuffer), indexBuffer(o.indexBuffer),
          indexCount_(o.indexCount_), allocator_(o.allocator_) {
        o.vertexBuffer = {}; o.indexBuffer = {};
        o.indexCount_ = 0; o.allocator_ = nullptr;
    }
    VkMeshData& operator=(VkMeshData&& o) noexcept {
        if (this != &o) {
            if (allocator_) {
                destroyBuffer(allocator_, vertexBuffer);
                destroyBuffer(allocator_, indexBuffer);
            }
            vertexBuffer = o.vertexBuffer; indexBuffer = o.indexBuffer;
            indexCount_ = o.indexCount_; allocator_ = o.allocator_;
            o.vertexBuffer = {}; o.indexBuffer = {};
            o.indexCount_ = 0; o.allocator_ = nullptr;
        }
        return *this;
    }

private:
    uint32_t     indexCount_ = 0;
    VmaAllocator allocator_  = nullptr;
};
