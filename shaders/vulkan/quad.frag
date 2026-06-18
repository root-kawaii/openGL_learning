#version 450

layout(location = 0) in  vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D screenTexture;
layout(set = 0, binding = 1) uniform sampler3D colorLUT;   // color-grade LUT
layout(set = 0, binding = 2) uniform sampler2D shadowMap;  // directional light depth
layout(set = 0, binding = 3) uniform VolumetricUBO {
    mat4 invViewProj;
    mat4 lightSpaceMatrix;
    vec4 camPos;       // xyz = camera world pos, w = depthMax
    vec4 sunDir;       // xyz = direction TO sun, w = volumetric strength
} vol;

// Stylized post-process parameters (see BlitPush in vk_renderer.h)
layout(push_constant) uniform BlitPush {
    vec4 p0;           // x=paletteSize, y=ditherStrength, z=saturation, w=contrast
    vec4 p1;           // x=brightness,  y=temperature,    z=tint,       w=vignette
    vec4 p2;           // x=outlineStr,  y=godrayStr,       z=bloomStr,   w=dofStr
    vec4 outlineColor; // rgb = outline color, w = aoStrength
    vec4 p4;           // x=sunScreenX,  y=sunScreenY,      z=time,       w=texelScale
    vec4 p5;           // x=lutStrength, yzw=sunColor
} pc;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

// 4x4 ordered Bayer matrix, returns threshold in [0,1)
float bayer4x4(vec2 p) {
    int x = int(mod(p.x, 4.0));
    int y = int(mod(p.y, 4.0));
    int idx = x + y * 4;
    float m[16] = float[16](
        0.0, 8.0, 2.0,10.0,
       12.0, 4.0,14.0, 6.0,
        3.0,11.0, 1.0, 9.0,
       15.0, 7.0,13.0, 5.0);
    return (m[idx] + 0.5) / 16.0;
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(screenTexture, 0));
    vec4 src   = texture(screenTexture, fragUV);
    vec3 color = src.rgb;
    float depth = src.a;        // linear depth proxy written by scene shaders (1 = far/sky)

    // ── Screen-space ambient occlusion (depth-proxy, range-checked) ───────────
    // Compares center depth against a neighbourhood average; pixels sitting in a
    // local recess (farther than their surroundings) get darkened. The range
    // check ignores big depth jumps so object silhouettes don't form halos.
    float aoStr = pc.outlineColor.w;
    if (aoStr > 0.001 && depth < 0.985) {
        const vec2 taps[8] = vec2[](
            vec2( 1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0,-1.0),
            vec2( 0.7, 0.7), vec2(-0.7, 0.7), vec2(0.7,-0.7), vec2(-0.7,-0.7));
        float sum = 0.0, cnt = 0.0;
        float range = 0.06;                 // ~5 world units at depthMax 80
        for (int i = 0; i < 8; i++) {
            float dN = texture(screenTexture, fragUV + taps[i] * texel * 2.5).a;
            if (abs(dN - depth) < range) { sum += dN; cnt += 1.0; }
        }
        if (cnt > 0.5) {
            float avg = sum / cnt;
            float ao  = clamp((depth - avg) * 28.0, 0.0, 1.0);
            color *= (1.0 - ao * aoStr);
        }
    }

    // ── Depth of field: blur pixels far from the focus plane (depth ~0 near) ──
    float dofStr = pc.p2.w;
    if (dofStr > 0.001) {
        float coc = clamp(depth * dofStr, 0.0, 1.0);
        if (coc > 0.02) {
            vec3 acc = color;
            float r = coc * 2.5;
            acc += texture(screenTexture, fragUV + vec2( texel.x, 0.0) * r).rgb;
            acc += texture(screenTexture, fragUV + vec2(-texel.x, 0.0) * r).rgb;
            acc += texture(screenTexture, fragUV + vec2(0.0,  texel.y) * r).rgb;
            acc += texture(screenTexture, fragUV + vec2(0.0, -texel.y) * r).rgb;
            color = mix(color, acc / 5.0, coc);
        }
    }

    // ── Stylized edge outline (depth + luminance discontinuity) ───────────────
    float outlineStr = pc.p2.x;
    if (outlineStr > 0.001) {
        float dC = depth;
        float dL = texture(screenTexture, fragUV - vec2(texel.x, 0.0)).a;
        float dR = texture(screenTexture, fragUV + vec2(texel.x, 0.0)).a;
        float dU = texture(screenTexture, fragUV - vec2(0.0, texel.y)).a;
        float dD = texture(screenTexture, fragUV + vec2(0.0, texel.y)).a;
        float depthEdge = abs(dL - dC) + abs(dR - dC) + abs(dU - dC) + abs(dD - dC);
        float lC = luma(color);
        float lL = luma(texture(screenTexture, fragUV - vec2(texel.x, 0.0)).rgb);
        float lR = luma(texture(screenTexture, fragUV + vec2(texel.x, 0.0)).rgb);
        float lumaEdge = abs(lL - lC) + abs(lR - lC);
        float edge = clamp(depthEdge * 14.0 + lumaEdge * 1.2, 0.0, 1.0);
        // Don't outline the sky (depth ~1 on both sides)
        edge *= step(dC, 0.985);
        color = mix(color, pc.outlineColor.rgb, edge * outlineStr);
    }

    // ── Volumetric light shafts (shadow-map raymarching) ───────────────────────
    float godrayStr = vol.sunDir.w;
    if (godrayStr > 0.001 && depth < 0.99) {
        // Reconstruct view ray: unproject UV to world-space direction
        // Vulkan fragUV.y is top-down but projection has Y-flip, so flip Y for NDC
        vec2 ndc = vec2(fragUV.x * 2.0 - 1.0, (1.0 - fragUV.y) * 2.0 - 1.0);
        vec4 worldFar = vol.invViewProj * vec4(ndc, 1.0, 1.0);
        vec3 farPt = worldFar.xyz / worldFar.w;
        vec4 worldNear = vol.invViewProj * vec4(ndc, 0.0, 1.0);
        vec3 nearPt = worldNear.xyz / worldNear.w;

        vec3 rayOrigin = vol.camPos.xyz;
        vec3 rayDir    = normalize(farPt - nearPt);
        // depth is viewDist / depthMax, so actual distance = depth * depthMax
        float viewDist = depth * vol.camPos.w;
        float marchLen = min(viewDist, 30.0);
        const int NUM_STEPS = 24;
        vec3  step_    = rayDir * (marchLen / float(NUM_STEPS));

        float litCount = 0.0;
        float shadCount = 0.0;
        vec3  pos     = rayOrigin;
        float dither  = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
        pos += step_ * dither;

        for (int i = 0; i < NUM_STEPS; i++) {
            vec4 lsPos = vol.lightSpaceMatrix * vec4(pos, 1.0);
            vec3 lsNDC = lsPos.xyz / lsPos.w;
            vec2 shadowUV = lsNDC.xy * 0.5 + 0.5;
            if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 &&
                shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
                float shadowDepth = texture(shadowMap, shadowUV).r;
                float lit = (lsNDC.z <= shadowDepth + 0.005) ? 1.0 : 0.0;
                litCount += lit;
                shadCount += 1.0;
            }
            pos += step_;
        }

        if (shadCount > 0.5) {
            float litRatio = litCount / shadCount;
            float shadRatio = 1.0 - litRatio;
            // Only show shafts where some occlusion exists (not pure open sky)
            float hasShadow = smoothstep(0.05, 0.25, shadRatio);
            float shaft = litRatio * hasShadow;

            vec3 sunCol = pc.p5.yzw;
            color += sunCol * shaft * godrayStr * 0.25;
        }
    }

    // Screen-space radial rays (supplement near sun)
    {
        vec2  sunUV = pc.p4.xy;
        float ssStr = vol.sunDir.w;
        if (ssStr > 0.001 &&
            sunUV.x > -0.3 && sunUV.x < 1.3 && sunUV.y > -0.3 && sunUV.y < 1.3) {
            const int STEPS = 16;
            vec2  delta = (fragUV - sunUV) / float(STEPS) * 0.85;
            vec2  uv    = fragUV;
            float illum = 0.0;
            float decay = 1.0;
            for (int i = 0; i < STEPS; i++) {
                uv -= delta;
                vec4 s = texture(screenTexture, uv);
                illum += max(luma(s.rgb) - 0.6, 0.0) * s.a * decay;
                decay *= 0.93;
            }
            illum /= float(STEPS);
            vec2 e = abs(sunUV - 0.5);
            float edge = clamp(1.0 - max(e.x, e.y) * 1.5, 0.0, 1.0);
            color += pc.p5.yzw * illum * ssStr * 1.0 * edge;
        }
    }

    // ── Stylized bloom: bright-pass + small blur, added back ───────────────────
    float bloomStr = pc.p2.z;
    if (bloomStr > 0.001) {
        vec3 b = vec3(0.0);
        float th = 0.82;   // only genuinely bright pixels bloom (avoid washing light walls)
        for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++) {
            vec3 s = texture(screenTexture, fragUV + vec2(x, y) * texel * 1.5).rgb;
            b += max(s - th, vec3(0.0));
        }
        color += (b / 9.0) * bloomStr * 1.2;
    }

    // ── Color grade: temperature / tint / saturation / contrast / brightness ──
    float temperature = pc.p1.y;
    float tint        = pc.p1.z;
    color.r += temperature * 0.10;
    color.b -= temperature * 0.10;
    color.g += tint * 0.10;
    float l = luma(color);
    color = mix(vec3(l), color, pc.p0.z);            // saturation
    color = (color - 0.5) * pc.p0.w + 0.5;           // contrast
    color += pc.p1.x;                                // brightness
    color = max(color, vec3(0.0));

    // ── Vignette ──────────────────────────────────────────────────────────────
    float vig = pc.p1.w;
    if (vig > 0.001) {
        vec2 d = fragUV - 0.5;
        float v = smoothstep(0.8, 0.2, dot(d, d) * 2.0);
        color *= mix(1.0, v, vig);
    }

    // ── Color-grade LUT (final palette remap before quantization) ─────────────
    float lutStrength = pc.p5.x;
    if (lutStrength > 0.001) {
        vec3 graded = texture(colorLUT, clamp(color, 0.0, 1.0)).rgb;
        color = mix(color, graded, lutStrength);
    }

    // ── Ordered dither, then posterize (the pixel-art crunch) ─────────────────
    float palette = pc.p0.x;
    if (palette > 0.0) {
        float dither = pc.p0.y;
        float texelScale = max(pc.p4.w, 1.0);
        float t = (bayer4x4(gl_FragCoord.xy / texelScale) - 0.5) * (dither / palette);
        color += t;
        color = floor(color * palette + 0.5) / palette;
    }

    outColor = vec4(color, 1.0);
}
