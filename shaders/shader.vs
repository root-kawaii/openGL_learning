#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;      // Add this
layout (location = 4) in vec3 aBitangent;    // Add this
layout (location = 5) in ivec4 boneIDs;      // Change from 3 to 5
layout (location = 6) in vec4 weights;       // Change from 3 to 6

 
out vec2 TexCoords;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    flat ivec4 boneIDs;
    vec4 weights;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;
uniform float time;

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

    vs_out.FragPos = worldPos.xyz;
    vs_out.Normal = transpose(inverse(mat3(model))) * NormalL.xyz;
    vs_out.TexCoords = aTexCoords;
    vs_out.FragPosLightSpace = lightSpaceMatrix * worldPos;
    vs_out.boneIDs = boneIDs;
    vs_out.weights = weights;

    gl_Position = projection * view * worldPos;
}