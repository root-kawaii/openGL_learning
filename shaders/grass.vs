#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 WorldPos;

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    
    // Pass to geometry shader in world space
    gl_Position = vec4(WorldPos, 1.0);
}