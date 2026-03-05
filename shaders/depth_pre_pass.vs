#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in ivec4 boneIDs;
layout (location = 6) in vec4 weights;

const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];
uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    mat4 BoneTransform = gBones[boneIDs[0]] * weights[0];
    BoneTransform     += gBones[boneIDs[1]] * weights[1];
    BoneTransform     += gBones[boneIDs[2]] * weights[2];
    BoneTransform     += gBones[boneIDs[3]] * weights[3];

    vec4 skinnedPos = BoneTransform * vec4(aPos, 1.0);
    gl_Position = lightSpaceMatrix * model * skinnedPos;
}
