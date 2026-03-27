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

// ── FBM wave displacement ─────────────────────────────────────────────────────
float hash(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1,0)), u.x),
               mix(hash(i + vec2(0,1)), hash(i + vec2(1,1)), u.x), u.y);
}

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * noise(p);
        p  = p * 2.1 + vec2(1.3, 1.7);
        a *= 0.5;
    }
    return v;
}

float waveY(vec2 xz) {
    vec2 animated = xz * 0.3 + push.time * push.waveSpeed * vec2(0.7, 0.5);
    return fbm(animated) * push.waveHeight + push.waterLevel;
}

void main() {
    vec2 xz  = inPosition.xz;
    float eps = 0.2;

    float hC = waveY(xz);
    float hL = waveY(xz + vec2(-eps, 0.0));
    float hR = waveY(xz + vec2( eps, 0.0));
    float hD = waveY(xz + vec2(0.0, -eps));
    float hU = waveY(xz + vec2(0.0,  eps));

    // Analytical normal from finite differences
    vec3 N = normalize(vec3(hL - hR, 2.0 * eps, hD - hU));

    vec3 worldPos = vec3(xz.x, hC, xz.y);

    fragWorldPos  = worldPos;
    fragNormal    = N;
    fragTexCoord  = inTexCoord;

    vec4 clip    = frame.proj * frame.view * vec4(worldPos, 1.0);
    fragPosClip  = clip;
    gl_Position  = clip;
}
