#version 450

// ── Vertex inputs ───────────────────────────────────────────────────────────
// These match the TexturedVertex struct in vk_pipeline.cpp:
//   location 0 = position (vec3)
//   location 1 = texCoord (vec2)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

// ── Outputs to fragment shader ──────────────────────────────────────────────
layout(location = 0) out vec2 fragTexCoord;

// ── Uniform buffer (same MVP as before) ─────────────────────────────────────
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}
