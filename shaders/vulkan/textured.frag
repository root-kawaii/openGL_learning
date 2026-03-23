#version 450

// ── Input from vertex shader ────────────────────────────────────────────────
layout(location = 0) in vec2 fragTexCoord;

// ── Output color ────────────────────────────────────────────────────────────
layout(location = 0) out vec4 outColor;

// ── Combined image sampler ──────────────────────────────────────────────────
// This is the Vulkan equivalent of a sampler2D uniform in OpenGL.
// "combined image sampler" = VkImageView + VkSampler bundled together.
// Bound at set 0, binding 1 (binding 0 is the UBO).
layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
    outColor = texture(texSampler, fragTexCoord);
}
