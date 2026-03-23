#version 450

// ── Vertex inputs ───────────────────────────────────────────────────────────
// Must match the Vertex struct layout (position, normal, texcoord, tangent,
// bitangent, boneIDs, weights). For Phase 5 we only use the first 3.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in ivec4 inBoneIDs;
layout(location = 6) in vec4 inWeights;

// ── Outputs to fragment shader ──────────────────────────────────────────────
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;

// ── Uniforms ────────────────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos  = worldPos.xyz;
    fragNormal    = mat3(transpose(inverse(ubo.model))) * inNormal;
    fragTexCoord  = inTexCoord;
    gl_Position   = ubo.proj * ubo.view * worldPos;
}
