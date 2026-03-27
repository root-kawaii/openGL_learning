#version 450

// ── Per-vertex blade geometry (binding 0) ────────────────────────────────────
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// ── Per-instance data (binding 1) ────────────────────────────────────────────
layout(location = 3) in vec3  instancePos;
layout(location = 4) in float instanceRotation;
layout(location = 5) in float instanceScale;
layout(location = 6) in float instanceHeightVar;
layout(location = 7) in vec3  instanceTint;

// ── Outputs ─────────────────────────────────────────────────────────────────
layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out float fragAlpha;

// ── Descriptors ─────────────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 lightPos;
    vec4 viewPos;
} frame;

// ── Push constant ────────────────────────────────────────────────────────────
layout(push_constant) uniform GrassPush {
    float time;
    float windStrength;
    float windSpeed;
    float grassHeight;
} push;

void main() {
    // Rotate blade around Y axis by instanceRotation
    float s = sin(instanceRotation);
    float c = cos(instanceRotation);
    mat3 rotY = mat3(
        c,  0.0, s,
        0.0, 1.0, 0.0,
       -s,  0.0, c
    );

    // Scale blade height by instanceScale + heightVar
    vec3 bladePos = inPos * vec3(instanceScale, instanceScale * (1.0 + instanceHeightVar), instanceScale);
    bladePos = rotY * bladePos;

    // Wind: apply sinusoidal displacement proportional to blade height (UV.y)
    // Upper part of blade sways more (inUV.y near 1.0)
    float windPhase = dot(instancePos.xz, vec2(1.3, 0.7)) + push.time * push.windSpeed;
    float windX     = sin(windPhase)       * push.windStrength * inUV.y * inUV.y;
    float windZ     = cos(windPhase * 0.7) * push.windStrength * inUV.y * inUV.y * 0.5;
    bladePos.x += windX;
    bladePos.z += windZ;

    vec3 worldPos = instancePos + bladePos;
    worldPos.y   += push.grassHeight;  // lift grass above ground offset

    fragUV    = inUV;
    fragColor = instanceTint;
    // Fade alpha near top of blade for soft tips
    fragAlpha = 1.0 - inUV.y * 0.3;

    gl_Position = frame.proj * frame.view * vec4(worldPos, 1.0);
}
