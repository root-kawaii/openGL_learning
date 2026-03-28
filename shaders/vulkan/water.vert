#version 450

// ── Inputs ──────────────────────────────────────────────────────────────────
layout(location = 0) in vec3 inPosition;   // XZ plane grid vertex
layout(location = 1) in vec2 inTexCoord;

// ── Outputs ─────────────────────────────────────────────────────────────────
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragPosClip;   // for reflection UV

// ── Descriptors ─────────────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 lightPos;
    vec4 viewPos;
} frame;

// ── Push constant ────────────────────────────────────────────────────────────
layout(push_constant) uniform WaterPush {
    float time;
    float waveHeight;
    float waveSpeed;
    float waterLevel;
} push;

// ── Sine × sine wave displacement ───────────────────────────────────────────
// Two perpendicular sine waves multiplied together create a natural ebb-and-flow
// motion: peaks and troughs move in a cross pattern, matching pixel art ocean style.

float waveDisp(vec2 xz) {
    float t     = push.time * push.waveSpeed;
    float freq  = 0.8;
    float wave1 = sin(xz.x * freq        + t);
    float wave2 = sin(xz.y * freq * 0.9  + t * 1.1);
    return wave1 * wave2 * push.waveHeight;
}

void main() {
    vec2 xz  = inPosition.xz;
    float eps = 0.3;

    float hC = waveDisp(xz);
    float hL = waveDisp(xz + vec2(-eps, 0.0));
    float hR = waveDisp(xz + vec2( eps, 0.0));
    float hD = waveDisp(xz + vec2(0.0, -eps));
    float hU = waveDisp(xz + vec2(0.0,  eps));

    // Normal from finite differences of the displacement function
    vec3 N = normalize(vec3((hL - hR) / (2.0 * eps),
                            1.0,
                            (hD - hU) / (2.0 * eps)));

    vec3 worldPos = vec3(xz.x, push.waterLevel + hC, xz.y);

    fragWorldPos  = worldPos;
    fragNormal    = N;
    fragTexCoord  = inTexCoord;

    vec4 clip    = frame.proj * frame.view * vec4(worldPos, 1.0);
    fragPosClip  = clip;
    gl_Position  = clip;
}
