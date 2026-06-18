#version 450

// ── Inputs ──────────────────────────────────────────────────────────────────
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// ── Output ───────────────────────────────────────────────────────────────────
layout(location = 0) out vec4 outColor;

// ── Descriptors ─────────────────────────────────────────────────────────────
struct PointLight { vec4 position; vec4 color; };
layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 lightPos;
    vec4 viewPos;
    PointLight pointLights[16];
    int  numPointLights;
    vec4 fogColor;
    vec4 fogParams;
    vec4 skyTop;
    vec4 skyHorizon;
    vec4 sunDir;   // xyz dir to sun
    vec4 style;
    vec4 rimColor;
} frame;

layout(set = 0, binding = 1) uniform sampler2D normalMap;
// Scene captured from the normal camera: rgb = color, a = linear distance from
// camera (dist / depthMax). Ray-marched for screen-space reflections.
layout(set = 0, binding = 2) uniform sampler2D sceneCapture;

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

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * noise(p);
        p *= 2.1;
        a *= 0.45;
    }
    return v;
}

// ── Screen-space reflection ──────────────────────────────────────────────────
// Reflect the view ray about the surface normal and march it through world space.
// Each step projects to screen with the same view-proj used to render the capture,
// then compares the ray's distance-from-camera against the captured distance
// (sceneCapture.a * depthMax) to find the geometry the ray actually hits. Returns
// the reflected color; 'edgeFade' (out) is 1 on a screen hit, 0 on a miss.
vec3 traceReflection(vec3 worldPos, vec3 viewDir, vec3 normal, out float edgeFade) {
    const int   STEPS    = 80;
    const float MAX_DIST = 45.0;
    const float STEP_LEN = MAX_DIST / float(STEPS);

    float depthMax = max(frame.style.w, 1.0);
    mat4  viewProj = frame.proj * frame.view;   // same Y-flipped VP as the capture
    vec3  camPos   = frame.viewPos.xyz;
    vec3  reflDir  = reflect(-viewDir, normal);

    vec3 rayPos  = worldPos + normal * 0.02;
    vec3 prevPos = rayPos;
    edgeFade = 0.0;

    for (int i = 0; i < STEPS; i++) {
        rayPos += reflDir * STEP_LEN;

        vec4 clip = viewProj * vec4(rayPos, 1.0);
        if (clip.w <= 0.0) break;
        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
        if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) break;

        float sceneDist = texture(sceneCapture, uv).a * depthMax;
        float behind    = length(rayPos - camPos) - sceneDist;

        // Ray crossed just behind a real surface (not the far/sky clear).
        if (behind > 0.0 && behind < STEP_LEN * 3.0 && sceneDist < depthMax * 0.999) {
            // Binary-search the crossing between prevPos (front) and rayPos (back)
            // to pin a crisp hit point instead of the coarse step.
            vec3 lo = prevPos, hi = rayPos, hit = rayPos;
            for (int k = 0; k < 6; k++) {
                vec3 mid = (lo + hi) * 0.5;
                vec4 mc  = viewProj * vec4(mid, 1.0);
                vec2 muv = (mc.xy / mc.w) * 0.5 + 0.5;
                float md = texture(sceneCapture, muv).a * depthMax;
                if (length(mid - camPos) > md) { hi = mid; hit = mid; }
                else                            lo = mid;
            }
            vec4 hc  = viewProj * vec4(hit, 1.0);
            vec2 huv = (hc.xy / hc.w) * 0.5 + 0.5;
            vec2 e   = abs(huv - 0.5);
            edgeFade = 1.0 - smoothstep(0.4, 0.5, max(e.x, e.y));
            return texture(sceneCapture, huv).rgb;
        }
        prevPos = rayPos;
    }
    return vec3(0.0);
}

void main() {
    vec3 N   = normalize(fragNormal);
    vec3 V   = normalize(frame.viewPos.xyz - fragWorldPos);
    float t  = push.time * push.waveSpeed;

    // ── Fresnel (subtle for dark indoor pool) ──────────────────────────────
    float NdotV  = max(dot(N, V), 0.0);
    float fresnel = pow(1.0 - NdotV, 5.0);
    fresnel = mix(0.25, 0.9, fresnel);   // sheen head-on, mirror-like at grazing

    // ── Noise layers (pool-scaled, ~8m) ─────────────────────────────────────
    vec2 xz = fragWorldPos.xz;

    // Slow swirl offsets — circular drift instead of linear to avoid sliding look
    float s1 = sin(t * 0.11) * 0.8;
    float c1 = cos(t * 0.13) * 0.8;
    float s2 = sin(t * 0.17 + 2.0) * 0.6;
    float c2 = cos(t * 0.14 + 1.0) * 0.6;

    float n_depth = fbm(xz * 0.5 + vec2(s1, c1));
    float n_detail = fbm(xz * 1.2 + vec2(s2, c2));
    float n_caustic = fbm(xz * 2.0 + vec2(-c1, s2) * 1.5);

    // ── Water color — derived from sky so it adapts to any scene ──────────────
    vec3 skyBase = frame.skyTop.rgb;
    float skyBright = dot(skyBase, vec3(0.299, 0.587, 0.114));
    vec3 shallowColor = mix(vec3(0.06, 0.12, 0.10), skyBase * vec3(0.25, 0.35, 0.45), skyBright);
    vec3 deepColor    = mix(vec3(0.02, 0.04, 0.06), skyBase * vec3(0.08, 0.12, 0.20), skyBright);
    float depthFade   = clamp(n_depth * 0.7 + 0.35, 0.0, 1.0);
    vec3 waterColor   = mix(shallowColor, deepColor, depthFade);

    waterColor += vec3(0.02, 0.04, 0.03) * (n_detail - 0.5);

    // ── Caustic highlights from nearby point lights ──────────────────────────
    float causticPattern = smoothstep(0.45, 0.55, n_caustic) * 0.5;
    vec3 causticLight = vec3(0.0);
    for (int i = 0; i < frame.numPointLights && i < 16; i++) {
        vec3 lpos = frame.pointLights[i].position.xyz;
        float dist = length(lpos - fragWorldPos);
        float atten = 1.0 / (1.0 + dist * 0.5 + dist * dist * 0.1);
        vec3 lcol = frame.pointLights[i].color.rgb * frame.pointLights[i].color.w;
        causticLight += lcol * atten;
    }
    waterColor += causticPattern * causticLight * 0.06;

    // ── Screen-space reflection ────────────────────────────────────────────────
    // Ripple the normal slightly so the reflection shimmers, then ray-march it.
    vec3  Nr  = normalize(N + vec3(n_detail - 0.5, 0.0, n_depth - 0.5) * 0.08);
    float hit = 0.0;
    vec3  reflection = traceReflection(fragWorldPos, V, Nr, hit);

    // Miss → fall back to a sky-tinted reflection so the surface still reads as water.
    vec3 skyRefl = mix(frame.skyHorizon.rgb, frame.skyTop.rgb, 0.5);
    reflection = mix(skyRefl, reflection, hit);
    reflection *= mix(vec3(0.8, 0.82, 0.85), vec3(0.95, 0.97, 1.0), skyBright);

    // ── Edge darkening — pool edges fade to black ────────────────────────────
    float edgeDist = length(xz) / 10.0;  // 10.0 = pool half-size
    float edgeFade = smoothstep(0.7, 1.0, edgeDist);

    // ── Composition ───────────────────────────────────────────────────────────
    vec3 color = waterColor;
    color = mix(color, reflection, fresnel);
    vec3 edgeColor = mix(vec3(0.01, 0.02, 0.02), deepColor, skyBright);
    color = mix(color, edgeColor, edgeFade * 0.6);

    // ── Point-light specular — torchlight glinting off the surface ───────────
    for (int i = 0; i < frame.numPointLights && i < 16; i++) {
        vec3 lpos = frame.pointLights[i].position.xyz;
        vec3 L = normalize(lpos - fragWorldPos);
        float dist = length(lpos - fragWorldPos);
        float atten = 1.0 / (1.0 + dist * 0.2 + dist * dist * 0.04);
        vec3 lcol = frame.pointLights[i].color.rgb * frame.pointLights[i].color.w;

        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 120.0);
        color += lcol * spec * atten * 0.25;
    }

    float alpha = mix(0.85, 0.95, depthFade);
    alpha = mix(alpha, 0.0, edgeFade);  // fade out at pool edges

    // ── Fog (match scene) ─────────────────────────────────────────────────────
    if (frame.fogColor.a > 0.0) {
        float viewDist  = length(frame.viewPos.xyz - fragWorldPos);
        float distFog   = smoothstep(frame.fogParams.x, frame.fogParams.y, viewDist);
        float heightFog = exp(-max(fragWorldPos.y - frame.fogParams.z, 0.0) * frame.fogParams.w);
        float fog = clamp(distFog * (1.0 + heightFog * 0.6) * frame.fogColor.a, 0.0, 1.0);
        color = mix(color, frame.fogColor.rgb, fog);
    }

    outColor = vec4(color, alpha);
}
