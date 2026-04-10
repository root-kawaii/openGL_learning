#version 450

// ── Vertex inputs ───────────────────────────────────────────────────────────
// Must match the Vertex struct layout (position, normal, texcoord, tangent,
// bitangent, boneIDs, weights).
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

// ── Per-frame data (UBO) ────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 lightPos;
} frame;

// ── Bone matrices (UBO) ────────────────────────────────────────────────────
const int MAX_BONES = 100;
layout(set = 0, binding = 3) uniform BoneUBO {
    mat4 bones[MAX_BONES];
} boneData;

// ── Per-object data (push constant) ─────────────────────────────────────────
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 albedoTint;
} push;

void main() {
    float totalWeight = inWeights[0] + inWeights[1] + inWeights[2] + inWeights[3];

    vec4 localPos;
    vec3 localNormal;

    if (totalWeight > 0.0) {
        // Skeletal animation: blend up to 4 bone transforms per vertex
        mat4 BoneTransform  = boneData.bones[inBoneIDs[0]] * inWeights[0];
        BoneTransform      += boneData.bones[inBoneIDs[1]] * inWeights[1];
        BoneTransform      += boneData.bones[inBoneIDs[2]] * inWeights[2];
        BoneTransform      += boneData.bones[inBoneIDs[3]] * inWeights[3];

        localPos    = BoneTransform * vec4(inPosition, 1.0);
        localNormal = mat3(BoneTransform) * inNormal;
    } else {
        // No bones — use bind pose
        localPos    = vec4(inPosition, 1.0);
        localNormal = inNormal;
    }

    vec4 worldPos = push.model * localPos;
    fragWorldPos  = worldPos.xyz;
    fragNormal    = mat3(transpose(inverse(push.model))) * localNormal;
    fragTexCoord  = inTexCoord;
    gl_Position   = frame.proj * frame.view * worldPos;
}
