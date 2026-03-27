#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

layout(push_constant) uniform UIPush {
    mat4 orthoProj;
    vec4 color;   // tint multiplier
} push;

void main() {
    gl_Position = push.orthoProj * vec4(inPos, 0.0, 1.0);
    fragUV = inUV;
}
