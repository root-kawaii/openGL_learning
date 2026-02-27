#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in ivec4 boneIDs;
layout (location = 6) in vec4 weights;

out vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;

out float viewDepth;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 clipPlane;

const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];

void main()
{
    // Calculate total weight
    float totalWeight = weights[0] + weights[1] + weights[2] + weights[3];

    vec4 PosL;
    vec4 NormalL;

    // Only apply bone transformation if vertex has bone weights
    if (totalWeight > 0.0)
    {
        // Calculate bone transformation
        mat4 BoneTransform = gBones[boneIDs[0]] * weights[0];
        BoneTransform += gBones[boneIDs[1]] * weights[1];
        BoneTransform += gBones[boneIDs[2]] * weights[2];
        BoneTransform += gBones[boneIDs[3]] * weights[3];

        // Apply bone transformation to position and normal
        PosL = BoneTransform * vec4(aPos, 1.0);
        NormalL = BoneTransform * vec4(aNormal, 0.0);
    }
    else
    {
        // No bones - use position and normal as-is
        PosL = vec4(aPos, 1.0);
        NormalL = vec4(aNormal, 0.0);
    }

    vec4 worldPos = model * PosL;
    gl_ClipDistance[0] = dot(worldPos, clipPlane);
    FragPos = worldPos.xyz;
    TexCoords = aTexCoords;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    Normal = normalMatrix * NormalL.xyz;

    vec3 viewPos = (view * worldPos).xyz;
    viewDepth = -viewPos.z; // View space depth (positive)

    gl_Position = projection * view * worldPos;
}

