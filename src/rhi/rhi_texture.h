#pragma once

#include <cstdint>
#include <string>

// ─── RHITexture ──────────────────────────────────────────────────────────────
//
// Abstract base for a GPU texture.
//
// In OpenGL: wraps a GLuint texture ID (created with glGenTextures).
// In Vulkan: wraps VkImage + VkImageView + VkSampler + VmaAllocation.
//
// The abstraction hides the huge difference in texture management:
//   - OpenGL: glTexImage2D uploads directly, format conversion is implicit
//   - Vulkan: staging buffer → layout transition → copy → another transition
//             plus explicit sampler and image view creation
//
// For now, this is a marker interface. Concrete types add backend-specific
// accessors (GL texture ID, VkImageView + VkSampler, etc.).

class RHITexture {
public:
    virtual ~RHITexture() = default;

    virtual uint32_t getWidth()  const = 0;
    virtual uint32_t getHeight() const = 0;
};
