#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in float fragAlpha;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D grassTexture;

void main() {
    vec4 texColor = texture(grassTexture, fragUV);

    // Alpha test — discard fully transparent pixels (blade edges)
    if (texColor.a < 0.15) discard;

    // Tint texture by per-instance color (seasonal variation)
    vec3 color = texColor.rgb * fragColor;

    // Simple subsurface scattering approximation: lighter at tips
    float sss = mix(0.6, 1.0, fragUV.y);
    color *= sss;

    outColor = vec4(color, texColor.a * fragAlpha);
}
