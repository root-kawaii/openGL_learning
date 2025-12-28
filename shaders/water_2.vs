#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in ivec4 boneIDs;
layout (location = 6) in vec4 weights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

out vec2 TexCoord;
out vec3 WorldPos;
out vec4 ClipSpacePos;

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    
    // Simple wave animation for water surface
    float wave = sin(WorldPos.x * 0.5 + time) * 0.05 + 
                cos(WorldPos.z * 0.3 + time * 1.2) * 0.015;
    WorldPos.y += wave;
    
    ClipSpacePos = projection * view * vec4(WorldPos, 1.0);
    gl_Position = ClipSpacePos;
    
    TexCoord = aTexCoord;
}