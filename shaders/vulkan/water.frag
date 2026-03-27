#version 450

// ── Inputs ──────────────────────────────────────────────────────────────────
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragPosClip;

// ── Output ───────────────────────────────────────────────────────────────────
layout(location = 0) out vec4 outColor;

// ── Descriptors ─────────────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 lightPos;
    vec4 viewPos;
} frame;

layout(set = 0, binding = 1) uniform sampler2D normalMap;    // tiling wave normal detail
layout(set = 0, binding = 2) uniform sampler2D reflectionTex; // planar reflection image

// ── Push constant ────────────────────────────────────────────────────────────
layout(push_constant) uniform WaterPush {
    float time;
    float waveHeight;
    float waveSpeed;
    float waterLevel;
} push;

void main() {
    vec3 N = normalize(fragNormal);

    // Two-layer animated normal map for surface detail
    vec2 uv1 = fragTexCoord * 4.0 + push.time * 0.03 * vec2(1.0, 0.7);
    vec2 uv2 = fragTexCoord * 6.0 + push.time * 0.02 * vec2(-0.6, 1.0);
    vec3 n1  = texture(normalMap, uv1).rgb * 2.0 - 1.0;
    vec3 n2  = texture(normalMap, uv2).rgb * 2.0 - 1.0;
    vec3 bumpN = normalize(N + (n1 + n2) * 0.15);

    vec3 V    = normalize(frame.viewPos.xyz - fragWorldPos);
    vec3 L    = normalize(frame.lightPos.xyz - fragWorldPos);
    vec3 H    = normalize(V + L);

    // Fresnel: more reflective at grazing angles
    float fresnel = pow(1.0 - max(dot(bumpN, V), 0.0), 3.0);
    fresnel = mix(0.05, 0.95, fresnel);

    // Planar reflection UV (project clip pos to [0,1])
    vec2 reflUV  = (fragPosClip.xy / fragPosClip.w) * 0.5 + 0.5;
    reflUV.y     = 1.0 - reflUV.y;               // Vulkan Y-flip
    reflUV      += bumpN.xz * 0.03;              // distort by normal
    reflUV       = clamp(reflUV, 0.001, 0.999);

    vec3 reflection = texture(reflectionTex, reflUV).rgb;

    // Deep water color
    vec3 waterColor = vec3(0.05, 0.15, 0.25);

    // Specular highlight
    float spec  = pow(max(dot(bumpN, H), 0.0), 128.0);
    vec3 specular = vec3(1.0) * spec * 0.8;

    // Blend: water color + reflection via Fresnel + specular
    vec3 color = mix(waterColor, reflection, fresnel) + specular;

    // Slight transparency in shallow areas (fake depth fade)
    float alpha = 0.75 + fresnel * 0.2;

    outColor = vec4(color, alpha);
}
