#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <string>

// ─── VulkanTexture ────────────────────────────────────────────────────────────
//
// Wraps the Vulkan objects needed to use an image as a shader texture:
//   - VkImage:      the GPU-side pixel data
//   - VkImageView:  how the shader "sees" the image (format, mip levels, etc.)
//   - VkSampler:    filtering and addressing modes (linear, repeat, clamp, etc.)
//   - VmaAllocation: the VMA memory backing the image
//
// In OpenGL, glGenTextures + glTexImage2D does all of this in one call.
// In Vulkan, each piece is explicit — you see exactly what the driver is doing.

struct VulkanTexture {
    VkImage        image      = VK_NULL_HANDLE;
    VmaAllocation  allocation = VK_NULL_HANDLE;
    VkImageView    imageView  = VK_NULL_HANDLE;
    VkSampler      sampler    = VK_NULL_HANDLE;
    uint32_t       width      = 0;
    uint32_t       height     = 0;
};

// Load an image file (PNG/JPG/TGA/etc.) and upload it to a VkImage.
// Uses stb_image to decode, then a staging buffer to transfer to GPU-local memory.
// Also creates the image view and sampler.
//
// The process:
//   1. Decode image from disk into CPU memory (stb_image)
//   2. Create a CPU-visible staging BUFFER and copy pixel data into it
//   3. Create a GPU-local VkImage in UNDEFINED layout
//   4. Transition image layout: UNDEFINED → TRANSFER_DST_OPTIMAL
//   5. Copy staging buffer → VkImage (vkCmdCopyBufferToImage)
//   6. Transition image layout: TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL
//   7. Create VkImageView and VkSampler
//
// Layout transitions are barriers that tell the GPU "this image is changing
// from one usage to another" — they ensure correct memory ordering.
VulkanTexture loadTexture(
    VmaAllocator  allocator,
    VkDevice      device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue       queue,
    const std::string& filepath);

// Load a texture from compressed image bytes already in memory (e.g. embedded GLB textures).
// data / dataSize are the raw PNG/JPG bytes from aiTexture::pcData / aiTexture::mWidth.
// srgb = true for albedo/diffuse, false for normal/metallic/roughness maps.
VulkanTexture loadTextureFromMemory(
    VmaAllocator  allocator,
    VkDevice      device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue       queue,
    const unsigned char* data,
    uint32_t dataSize,
    bool srgb = true);

// Load a cubemap from 6 face images (right, left, top, bottom, front, back).
// Creates a VK_IMAGE_TYPE_2D with VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
// 6 array layers, and a VK_IMAGE_VIEW_TYPE_CUBE view.
VulkanTexture loadCubemap(
    VmaAllocator  allocator,
    VkDevice      device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue       queue,
    const std::array<std::string, 6>& faces);

// Destroy all texture resources
void destroyTexture(VmaAllocator allocator, VkDevice device, VulkanTexture& texture);
