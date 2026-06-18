#version 450

// ── Inputs ──────────────────────────────────────────────────────────────────
layout(location = 0) in vec3 inPosition;   // XZ plane grid vertex
layout(location = 1) in vec2 inTexCoord;

// ── Outputs ─────────────────────────────────────────────────────────────────
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

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

// Standing-wave displacement: multiple overlapping modes that oscillate in
// place rather than traveling, so the pool surface bobs without drifting.
float waveDisp(vec2 xz) {
    float t = push.time * push.waveSpeed;
    float h = 0.0;
    // Mode 1: broad swell
    h += sin(xz.x * 0.9) * sin(xz.y * 0.7) * cos(t * 1.0) * 0.45;
    // Mode 2: cross ripple
    h += sin(xz.x * 1.6 + xz.y * 1.2) * cos(t * 1.7) * 0.25;
    // Mode 3: fine detail
    h += sin(xz.x * 2.5 - xz.y * 2.1) * cos(t * 2.3 + 1.0) * 0.15;
    // Mode 4: diagonal wobble
    h += cos(xz.x * 1.1 - xz.y * 1.8) * sin(t * 1.3 + 2.5) * 0.15;
    return h * push.waveHeight;
}

void main() {
    vec2 xz  = inPosition.xz;
    float eps = 0.15;

    float hC = waveDisp(xz);
    float hL = waveDisp(xz + vec2(-eps, 0.0));
    float hR = waveDisp(xz + vec2( eps, 0.0));
    float hD = waveDisp(xz + vec2(0.0, -eps));
    float hU = waveDisp(xz + vec2(0.0,  eps));

    vec3 N = normalize(vec3((hL - hR) / (2.0 * eps),
                            1.0,
                            (hD - hU) / (2.0 * eps)));

    vec3 worldPos = vec3(xz.x, push.waterLevel + hC, xz.y);

    fragWorldPos  = worldPos;
    fragNormal    = N;
    fragTexCoord  = inTexCoord;

    gl_Position   = frame.proj * frame.view * vec4(worldPos, 1.0);
}
