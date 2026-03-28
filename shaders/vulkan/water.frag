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

layout(set = 0, binding = 1) uniform sampler2D normalMap;     // unused — reserved for future detail
layout(set = 0, binding = 2) uniform sampler2D reflectionTex; // planar reflection image

// ── Push constant ────────────────────────────────────────────────────────────
layout(push_constant) uniform WaterPush {
    float time;
    float waveHeight;
    float waveSpeed;
    float waterLevel;
} push;

// ── Noise ────────────────────────────────────────────────────────────────────

float hash(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i),           hash(i + vec2(1.0, 0.0)), u.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

void main() {
    vec3 N   = normalize(fragNormal);
    vec3 V   = normalize(frame.viewPos.xyz - fragWorldPos);
    float t  = push.time * push.waveSpeed;

    // ── Fresnel ───────────────────────────────────────────────────────────────
    float NdotV  = max(dot(N, V), 0.0);
    float fresnel = pow(1.0 - NdotV, 3.0);
    fresnel = mix(0.05, 0.85, fresnel);

    // ── Noise layers (world-space UVs for seamless tiling) ───────────────────
    vec2 xz = fragWorldPos.xz;

    // Slow drift offsets for each layer
    vec2 drift1 = vec2( t * 0.20,  t * 0.15);
    vec2 drift2 = vec2(-t * 0.18,  t * 0.22);
    vec2 drift3 = vec2( t * 0.40, -t * 0.30);
    vec2 drift4 = vec2( t * 0.55, -t * 0.35);

    // Low-freq: depth color variation
    float n_depth  = noise(xz * 0.15 + drift1);

    // Med-freq pair: surface foam dots (both must exceed threshold)
    float n_foam1  = noise(xz * 0.50 + drift3);
    float n_foam2  = noise(xz * 0.75 + drift4);

    // High-freq: refraction jitter
    float n_refr   = noise(xz * 0.30 + drift2);

    // ── Depth gradient ────────────────────────────────────────────────────────
    vec3 shallowColor = vec3(0.30, 0.75, 0.85);  // light cyan
    vec3 deepColor    = vec3(0.02, 0.12, 0.35);  // dark navy
    float depthFade   = clamp(n_depth * 0.9 + 0.25, 0.0, 1.0);
    vec3 waterColor   = mix(shallowColor, deepColor, depthFade);

    // ── Planar reflection UV ──────────────────────────────────────────────────
    vec2 reflUV = (fragPosClip.xy / fragPosClip.w) * 0.5 + 0.5;
    reflUV.y    = 1.0 - reflUV.y;   // Vulkan Y-flip

    // Refraction: distort UV with wave normal + noise; stronger in shallow areas
    vec2 refractOffset = N.xz * 0.04
                       + vec2(n_refr - 0.5, n_depth - 0.5) * 0.025 * (1.0 - depthFade);
    reflUV = clamp(reflUV + refractOffset, 0.001, 0.999);

    vec3 reflection = texture(reflectionTex, reflUV).rgb;

    // ── Shore foam ────────────────────────────────────────────────────────────
    // The sine×sine product goes negative in troughs — that's where wave retreats,
    // exposing foam. We use the same formula as the vertex shader.
    float wave1     = sin(xz.x * 0.8 + t);
    float wave2     = sin(xz.y * 0.72 + t * 1.1);
    float wavePhase = wave1 * wave2;  // -1 .. 1

    // Foam band: narrow region around troughs, masked by high-freq noise
    float shoreFoam = smoothstep(-0.25, -0.05, wavePhase)  // fade in near trough
                    * smoothstep( 0.10, -0.05, wavePhase)  // fade out past trough
                    * step(0.50, n_foam1);                  // noise mask to break up line

    // ── Surface foam dots ─────────────────────────────────────────────────────
    float foamMask = step(0.70, n_foam1) * step(0.65, n_foam2);

    vec3 foamColor = vec3(0.94, 0.97, 1.0);

    // ── Composition ───────────────────────────────────────────────────────────
    vec3 color = waterColor;
    color = mix(color, reflection, fresnel * 0.6);
    color = mix(color, foamColor, shoreFoam * 0.85);
    color = mix(color, foamColor, foamMask  * 0.70);

    float alpha = mix(0.70, 0.95, depthFade);

    outColor = vec4(color, alpha);
}
