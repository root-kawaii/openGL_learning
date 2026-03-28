#version 450

layout(location = 0) in  vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D screenTexture;

layout(push_constant) uniform BlitPush {
    float paletteSize;  // color quantization levels per channel (e.g. 8, 16, 32)
} push;

void main() {
    vec4 color = texture(screenTexture, fragUV);
    // Posterize: quantize each channel to paletteSize steps
    color.rgb = floor(color.rgb * push.paletteSize + 0.5) / push.paletteSize;
    outColor = color;
}
