#include "vk_texture.h"
#include "vk_buffer.h"
#include <stb_image.h>
#include <iostream>
#include <cstring>

// ─── Image Layout Transitions ────────────────────────────────────────────────
//
// Vulkan images have a "layout" that describes what they're currently being
// used for. Before you can use an image in a new way (e.g. as a transfer
// destination, or as a shader texture), you must transition its layout.
//
// A layout transition is recorded as a pipeline barrier in a command buffer.
// The barrier specifies:
//   - srcStageMask / dstStageMask: which pipeline stages to synchronize
//   - srcAccessMask / dstAccessMask: which memory accesses to synchronize
//   - oldLayout / newLayout: the actual layout change
//
// This ensures the GPU finishes all reads/writes in the old layout before
// starting to use the image in the new layout.

static void transitionImageLayout(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout)
{
    // Allocate a one-shot command buffer for the barrier
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

    // ── Build the image memory barrier ──────────────────────────────────────
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    // Figure out the correct stage/access masks based on the transition.
    // Each transition type requires different synchronization:
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // Transition for receiving data from a buffer copy.
        // No prior access to wait for (image was UNDEFINED).
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // Transition from transfer destination to shader-readable.
        // Wait for transfer writes to complete before fragment shader reads.
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        std::cerr << "[Vulkan] Unsupported layout transition" << std::endl;
        vkEndCommandBuffer(cmd);
        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
        return;
    }

    vkCmdPipelineBarrier(cmd,
        srcStage, dstStage,
        0,                      // dependency flags
        0, nullptr,             // memory barriers
        0, nullptr,             // buffer memory barriers
        1, &barrier);           // image memory barriers

    vkEndCommandBuffer(cmd);

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue); // Fine for init-time uploads

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

// ─── Copy Buffer to Image ────────────────────────────────────────────────────
//
// After transitioning the image to TRANSFER_DST_OPTIMAL, we can copy pixel
// data from the staging buffer into the VkImage. This is similar to
// glTexSubImage2D in OpenGL — it uploads pixel data to GPU texture memory.

static void copyBufferToImage(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height)
{
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

    // Describe which region of the buffer maps to which region of the image
    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;  // 0 = tightly packed
    region.bufferImageHeight = 0;  // 0 = tightly packed
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

// ─── Load Texture ────────────────────────────────────────────────────────────

VulkanTexture loadTexture(
    VmaAllocator  allocator,
    VkDevice      device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue       queue,
    const std::string& filepath)
{
    VulkanTexture texture{};

    // ── 1. Load image from disk with stb_image ──────────────────────────────
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        std::cerr << "[Vulkan] Failed to load texture: " << filepath << std::endl;
        return texture;
    }

    texture.width  = static_cast<uint32_t>(texWidth);
    texture.height = static_cast<uint32_t>(texHeight);
    VkDeviceSize imageSize = texture.width * texture.height * 4; // RGBA = 4 bytes per pixel

    std::cout << "[Vulkan] Loaded texture: " << filepath
              << " (" << texture.width << "x" << texture.height << ")" << std::endl;

    // ── 2. Create CPU-visible staging buffer and copy pixel data ────────────
    AllocatedBuffer staging = createBuffer(
        allocator, imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY);

    void* mapped;
    vmaMapMemory(allocator, staging.allocation, &mapped);
    memcpy(mapped, pixels, static_cast<size_t>(imageSize));
    vmaUnmapMemory(allocator, staging.allocation);

    stbi_image_free(pixels); // CPU copy no longer needed

    // ── 3. Create the VkImage (GPU-local) ───────────────────────────────────
    //
    // VkImage is the GPU-side storage. We specify:
    //   - Format: R8G8B8A8_SRGB (sRGB for correct gamma)
    //   - Usage: TRANSFER_DST (we'll copy into it) + SAMPLED (shader will read it)
    //   - Tiling: OPTIMAL (GPU-native layout, not linear row-by-row)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = texture.width;
    imageInfo.extent.height = texture.height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(allocator, &imageInfo, &allocInfo,
                       &texture.image, &texture.allocation, nullptr) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create texture image" << std::endl;
        destroyBuffer(allocator, staging);
        return texture;
    }

    // ── 4. Transition layout: UNDEFINED → TRANSFER_DST_OPTIMAL ─────────────
    // The image starts in UNDEFINED (garbage). We need TRANSFER_DST before
    // we can copy the staging buffer into it.
    transitionImageLayout(device, commandPool, queue, texture.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // ── 5. Copy staging buffer → VkImage ────────────────────────────────────
    copyBufferToImage(device, commandPool, queue,
        staging.buffer, texture.image, texture.width, texture.height);

    // ── 6. Transition layout: TRANSFER_DST → SHADER_READ_ONLY ──────────────
    // Now the image has pixel data. Transition to SHADER_READ_ONLY so the
    // fragment shader can sample from it.
    transitionImageLayout(device, commandPool, queue, texture.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Done with staging buffer
    destroyBuffer(allocator, staging);

    // ── 7. Create VkImageView ───────────────────────────────────────────────
    //
    // An image view describes how to interpret the image data — which mip
    // levels, array layers, and color channels to use. Even for a simple
    // 2D texture we need an explicit view.
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = texture.image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &texture.imageView) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create texture image view" << std::endl;
        return texture;
    }

    // ── 8. Create VkSampler ─────────────────────────────────────────────────
    //
    // The sampler controls how the texture is filtered and addressed:
    //   - magFilter/minFilter: LINEAR = bilinear filtering (smooth)
    //   - addressMode: REPEAT = texture wraps around (like GL_REPEAT)
    //   - anisotropyEnable: anisotropic filtering for better quality at angles
    //
    // In OpenGL this is part of the texture object (glTexParameteri).
    // In Vulkan, samplers are separate objects — you can reuse one sampler
    // with multiple textures.

    // Query device for max anisotropy support
    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = deviceProps.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;  // Use [0,1] UV range
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create texture sampler" << std::endl;
        return texture;
    }

    std::cout << "[Vulkan] Texture ready (image + view + sampler)" << std::endl;
    return texture;
}

// ─── Cubemap Loading ─────────────────────────────────────────────────────────

VulkanTexture loadCubemap(
    VmaAllocator allocator,
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue queue,
    const std::array<std::string, 6>& faces)
{
    VulkanTexture tex{};

    // 1. Load all 6 faces and verify they have the same dimensions
    int width = 0, height = 0;
    std::array<unsigned char*, 6> pixels{};

    for (int i = 0; i < 6; i++) {
        int w, h, ch;
        pixels[i] = stbi_load(faces[i].c_str(), &w, &h, &ch, 4); // force RGBA
        if (!pixels[i]) {
            std::cerr << "[Vulkan] Failed to load cubemap face: " << faces[i] << std::endl;
            for (int j = 0; j < i; j++) stbi_image_free(pixels[j]);
            return tex;
        }
        if (i == 0) { width = w; height = h; }
        else if (w != width || h != height) {
            std::cerr << "[Vulkan] Cubemap face size mismatch: " << faces[i] << std::endl;
            for (int j = 0; j <= i; j++) stbi_image_free(pixels[j]);
            return tex;
        }
    }

    tex.width  = static_cast<uint32_t>(width);
    tex.height = static_cast<uint32_t>(height);
    VkDeviceSize faceSize  = width * height * 4;
    VkDeviceSize totalSize = faceSize * 6;

    // 2. Create staging buffer with all 6 faces packed sequentially
    AllocatedBuffer staging = createBuffer(
        allocator, totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* mapped;
    vmaMapMemory(allocator, staging.allocation, &mapped);
    for (int i = 0; i < 6; i++) {
        memcpy(static_cast<char*>(mapped) + i * faceSize, pixels[i], faceSize);
        stbi_image_free(pixels[i]);
    }
    vmaUnmapMemory(allocator, staging.allocation);

    // 3. Create cube image (6 array layers)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {tex.width, tex.height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 6;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(allocator, &imageInfo, &allocCreateInfo,
                        &tex.image, &tex.allocation, nullptr) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create cubemap image" << std::endl;
        destroyBuffer(allocator, staging);
        return tex;
    }

    // 4. Transition, copy, transition — all in one command buffer
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

    // Transition UNDEFINED → TRANSFER_DST (all 6 layers)
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = tex.image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 6;
    barrier.srcAccessMask                   = 0;
    barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy each face from staging buffer to the corresponding array layer
    std::array<VkBufferImageCopy, 6> regions{};
    for (int i = 0; i < 6; i++) {
        regions[i].bufferOffset      = i * faceSize;
        regions[i].bufferRowLength   = 0;
        regions[i].bufferImageHeight = 0;
        regions[i].imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[i].imageSubresource.mipLevel       = 0;
        regions[i].imageSubresource.baseArrayLayer = i;
        regions[i].imageSubresource.layerCount     = 1;
        regions[i].imageOffset = {0, 0, 0};
        regions[i].imageExtent = {tex.width, tex.height, 1};
    }

    vkCmdCopyBufferToImage(cmd, staging.buffer, tex.image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            6, regions.data());

    // Transition TRANSFER_DST → SHADER_READ_ONLY (all 6 layers)
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    destroyBuffer(allocator, staging);

    // 5. Create cube image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = tex.image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 6;

    if (vkCreateImageView(device, &viewInfo, nullptr, &tex.imageView) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create cubemap image view" << std::endl;
        return tex;
    }

    // 6. Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &tex.sampler) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create cubemap sampler" << std::endl;
        return tex;
    }

    std::cout << "[Vulkan] Cubemap loaded (" << width << "x" << height << ")" << std::endl;
    return tex;
}

void destroyTexture(VmaAllocator allocator, VkDevice device, VulkanTexture& texture) {
    if (texture.sampler)   { vkDestroySampler(device, texture.sampler, nullptr);     texture.sampler = VK_NULL_HANDLE; }
    if (texture.imageView) { vkDestroyImageView(device, texture.imageView, nullptr); texture.imageView = VK_NULL_HANDLE; }
    if (texture.image)     { vmaDestroyImage(allocator, texture.image, texture.allocation); texture.image = VK_NULL_HANDLE; texture.allocation = VK_NULL_HANDLE; }
}
