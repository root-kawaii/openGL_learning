#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

// RGBA sprite texture
layout(set = 0, binding = 0) uniform sampler2D spriteTexture;

layout(push_constant) uniform UIPush {
    mat4 orthoProj;
    vec4 color;
} push;

void main() {
    outColor = texture(spriteTexture, fragUV) * push.color;
}
