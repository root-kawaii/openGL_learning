#include "vk_buffer.h"
#include <iostream>
#include <cstring>

VmaAllocator createAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.instance       = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device         = device;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;

    VmaAllocator allocator;
    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create VMA allocator" << std::endl;
        return VK_NULL_HANDLE;
    }
    return allocator;
}

AllocatedBuffer createBuffer(
    VmaAllocator allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = size;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    // For CPU-visible buffers, request persistent mapping
    if (memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU ||
        memoryUsage == VMA_MEMORY_USAGE_CPU_ONLY) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    AllocatedBuffer result{};
    if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                        &result.buffer, &result.allocation, nullptr) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create buffer" << std::endl;
    }
    return result;
}

// ─── Staging Upload ──────────────────────────────────────────────────────────
//
// GPU-local memory (DEVICE_LOCAL) is fastest for the GPU to read but the CPU
// can't write to it directly. So we use a two-step process:
//
//   1. Create a CPU-visible "staging" buffer and copy data into it
//   2. Record a GPU command to copy from staging → device-local buffer
//   3. Submit, wait, destroy the staging buffer
//
// This is how vertex/index data gets to the GPU in Vulkan.

AllocatedBuffer createBufferWithStaging(
    VmaAllocator allocator,
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    const void* data,
    VkDeviceSize size,
    VkBufferUsageFlags usage)
{
    // 1. Create staging buffer (CPU-visible)
    AllocatedBuffer staging = createBuffer(
        allocator, size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY);

    // Copy data to staging buffer
    void* mapped;
    vmaMapMemory(allocator, staging.allocation, &mapped);
    memcpy(mapped, data, size);
    vmaUnmapMemory(allocator, staging.allocation);

    // 2. Create device-local buffer (GPU-only, fast)
    AllocatedBuffer result = createBuffer(
        allocator, size,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // 3. Copy staging → device-local via a one-shot command buffer
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool        = commandPool;
    cmdAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, staging.buffer, result.buffer, 1, &copyRegion);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue); // Simple sync — fine for init-time uploads

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    destroyBuffer(allocator, staging);

    return result;
}

void destroyBuffer(VmaAllocator allocator, AllocatedBuffer& buffer) {
    if (buffer.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
        buffer.buffer     = VK_NULL_HANDLE;
        buffer.allocation = VK_NULL_HANDLE;
    }
}
