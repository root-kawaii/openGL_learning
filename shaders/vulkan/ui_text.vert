#version 450

// Per-vertex: quad corner position + UV into glyph atlas
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

// Orthographic projection pushed per draw call
layout(push_constant) uniform UIPush {
    mat4  orthoProj;
    vec4  color;     // rgba text color
} push;

void main() {
    gl_Position = push.orthoProj * vec4(inPos, 0.0, 1.0);
    fragUV = inUV;
}
