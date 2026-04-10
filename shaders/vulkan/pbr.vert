#version 450

// ── Vertex inputs (match Vertex struct) ────────────────────────────────────
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in ivec4 inBoneIDs;
layout(location = 6) in vec4  inWeights;

// ── Outputs ────────────────────────────────────────────────────────────────
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragPosLightSpace;
layout(location = 4) out mat3 fragTBN;   // occupies locations 4, 5, 6

// ── FrameUBO ───────────────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 lightPos;
    vec4 viewPos;   // xyz = camera position
} frame;

// ── BoneUBO ────────────────────────────────────────────────────────────────
const int MAX_BONES = 100;
layout(set = 0, binding = 6) uniform BoneUBO {
    mat4 bones[MAX_BONES];
} boneData;

// ── Push constant ──────────────────────────────────────────────────────────
layout(push_constant) uniform PBRPush {
    mat4  model;
    float metallicVal;
    float roughnessVal;
    uint  hasNormalMap;
    uint  _pad;
    vec4  albedoTint;
} push;

void main() {
    float totalWeight = inWeights[0] + inWeights[1] + inWeights[2] + inWeights[3];

    vec4 localPos;
    vec3 localNormal;
    vec3 localTangent;

    if (totalWeight > 0.0) {
        mat4 BoneTransform  = boneData.bones[inBoneIDs[0]] * inWeights[0];
        BoneTransform      += boneData.bones[inBoneIDs[1]] * inWeights[1];
        BoneTransform      += boneData.bones[inBoneIDs[2]] * inWeights[2];
        BoneTransform      += boneData.bones[inBoneIDs[3]] * inWeights[3];

        localPos     = BoneTransform * vec4(inPosition, 1.0);
        localNormal  = mat3(BoneTransform) * inNormal;
        localTangent = mat3(BoneTransform) * inTangent;
    } else {
        localPos     = vec4(inPosition, 1.0);
        localNormal  = inNormal;
        localTangent = inTangent;
    }

    vec4 worldPos4 = push.model * localPos;
    fragWorldPos   = worldPos4.xyz;
    fragTexCoord   = inTexCoord;

    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    vec3 N = normalize(normalMatrix * localNormal);
    vec3 T = normalize(normalMatrix * localTangent);
    T = normalize(T - dot(T, N) * N);   // Gram-Schmidt
    vec3 B = cross(N, T);
    fragTBN    = mat3(T, B, N);
    fragNormal = N;

    fragPosLightSpace = frame.lightSpaceMatrix * worldPos4;

    gl_Position = frame.proj * frame.view * worldPos4;
}
