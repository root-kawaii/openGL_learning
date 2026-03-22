#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

// ─── AllocatedBuffer ─────────────────────────────────────────────────────────
//
// VMA (Vulkan Memory Allocator) simplifies Vulkan's manual memory management.
// In raw Vulkan you must: create buffer → query memory requirements → find a
// compatible memory type → allocate memory → bind buffer to memory. VMA does
// all of that in one call.
//
// AllocatedBuffer pairs a VkBuffer with its VMA allocation so we can easily
// create, map, and destroy buffers.

struct AllocatedBuffer {
    VkBuffer      buffer     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
};

// Create a VMA allocator tied to our device
VmaAllocator createAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

// Create a buffer with VMA
AllocatedBuffer createBuffer(
    VmaAllocator allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VmaMemoryUsage memoryUsage);

// Upload data to GPU via a staging buffer (CPU-visible → GPU-only copy)
AllocatedBuffer createBufferWithStaging(
    VmaAllocator allocator,
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    const void* data,
    VkDeviceSize size,
    VkBufferUsageFlags usage);

// Destroy a buffer and free its memory
void destroyBuffer(VmaAllocator allocator, AllocatedBuffer& buffer);
