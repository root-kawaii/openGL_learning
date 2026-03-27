#version 450

// ── Vertex inputs (same layout as model.vert) ──────────────────────────────
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in ivec4 inBoneIDs;
layout(location = 6) in vec4 inWeights;

// ── Per-frame data (UBO) ────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 lightPos;
} frame;

// ── Bone matrices (UBO) ────────────────────────────────────────────────────
const int MAX_BONES = 100;
layout(set = 0, binding = 1) uniform BoneUBO {
    mat4 bones[MAX_BONES];
} boneData;

// ── Push constant: model matrix + object ID ─────────────────────────────────
layout(push_constant) uniform PushConstants {
    mat4 model;
    uint objectID;
} push;

void main() {
    float totalWeight = inWeights[0] + inWeights[1] + inWeights[2] + inWeights[3];

    vec4 localPos;
    if (totalWeight > 0.0) {
        mat4 BoneTransform  = boneData.bones[inBoneIDs[0]] * inWeights[0];
        BoneTransform      += boneData.bones[inBoneIDs[1]] * inWeights[1];
        BoneTransform      += boneData.bones[inBoneIDs[2]] * inWeights[2];
        BoneTransform      += boneData.bones[inBoneIDs[3]] * inWeights[3];
        localPos = BoneTransform * vec4(inPosition, 1.0);
    } else {
        localPos = vec4(inPosition, 1.0);
    }

    gl_Position = frame.proj * frame.view * push.model * localPos;
}
