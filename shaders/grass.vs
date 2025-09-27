#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;

out vec3 WorldPos;

void main()
{
    // Transform to world space
    WorldPos = vec3(model * vec4(aPos, 1.0));
    
    // For geometry shader input, just pass world position
    gl_Position = vec4(WorldPos, 1.0);
}