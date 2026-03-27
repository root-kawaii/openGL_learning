#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

// R8_UNORM glyph atlas — red channel is coverage/alpha
layout(set = 0, binding = 0) uniform sampler2D glyphAtlas;

layout(push_constant) uniform UIPush {
    mat4  orthoProj;
    vec4  color;
} push;

void main() {
    float alpha = texture(glyphAtlas, fragUV).r;
    outColor = vec4(push.color.rgb, push.color.a * alpha);
}
