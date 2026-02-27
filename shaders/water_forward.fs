#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 ClipSpacePos;
in vec4 ReflClipPos;  // projected through reflected camera VP

// Textures
uniform sampler2D normalMap;        // water_normal.png — tangent-space normal map
uniform sampler2D foamTexture;      // foam.png         — foam pattern mask
uniform sampler2D reflectionTexture;// planar reflection FBO (half-res)

// Camera
uniform vec3  cameraWorldPos;
uniform float time;
uniform float surfaceLevel;

// Tunable water appearance
uniform vec3  waterColorShallow;
uniform vec3  waterColorDeep;
uniform float waterAlpha;
uniform float reflectStrength;
uniform float specStrength;
uniform float foamStrength;

// Sun
uniform vec3 sunDir;
uniform vec3 sunColor;

// ---------------------------------------------------------------------------
// Procedural sky — identical to cubemap.fs so reflections match
// ---------------------------------------------------------------------------
vec3 proceduralSky(vec3 dir)
{
    dir = normalize(dir);
    float y = dir.y;

    vec3 zenith  = vec3(0.10, 0.04, 0.28);
    vec3 midSky  = vec3(0.38, 0.10, 0.52);
    vec3 horizon = vec3(0.95, 0.38, 0.28);
    vec3 ground  = vec3(0.05, 0.02, 0.18);

    vec3 sky;
    if (y >= 0.0) {
        float t = pow(y, 0.55);
        sky = mix(horizon, mix(midSky, zenith, t), t);
    } else {
        sky = mix(ground, horizon, clamp(1.0 + y * 5.0, 0.0, 1.0));
    }

    vec3 sd_dir = normalize(vec3(0.55, 0.08, 0.40));
    float sd = dot(dir, sd_dir);
    sky += vec3(1.00, 0.95, 0.80) * smoothstep(0.9993, 1.000, sd);
    sky += vec3(1.00, 0.55, 0.15) * smoothstep(0.988, 0.9993, sd) * 0.80;
    sky += vec3(0.85, 0.25, 0.50) * smoothstep(0.950, 0.988,  sd) * 0.45;
    sky += vec3(0.45, 0.10, 0.55) * smoothstep(0.880, 0.950,  sd) * 0.25;

    float horizonBand = exp(-abs(y) * 4.5);
    float sunXZ = clamp(dot(normalize(dir.xz), normalize(sd_dir.xz)), 0.0, 1.0);
    sky += vec3(1.00, 0.45, 0.10) * horizonBand * sunXZ * 0.40;

    float antiSunXZ = clamp(dot(normalize(dir.xz), -normalize(sd_dir.xz)), 0.0, 1.0);
    sky += vec3(0.20, 0.05, 0.40) * horizonBand * antiSunXZ * 0.20;

    return sky;
}

// ---------------------------------------------------------------------------
void main()
{
    float t = time;

    // ---- 1. Wave-height factor (drives crest/trough color contrast) ------
    float displacement = FragPos.y - surfaceLevel;
    float crestFactor  = clamp(displacement * 3.5 + 0.5, 0.0, 1.0);

    // ---- 2. Normal map — two layers scrolling in different directions -----
    // Use world XZ as UV so texture tiles consistently across the large plane.
    vec2 uv1 = FragPos.xz * 0.04 + vec2( t * 0.018,  t * 0.013);
    vec2 uv2 = FragPos.xz * 0.07 + vec2(-t * 0.015,  t * 0.020);
    vec2 uv3 = FragPos.xz * 0.02 + vec2( t * 0.008, -t * 0.007); // large swell

    // Unpack tangent-space normals (X=right, Y=fwd in tangent → map to world XZ)
    vec3 n1 = texture(normalMap, uv1).rgb * 2.0 - 1.0;
    vec3 n2 = texture(normalMap, uv2).rgb * 2.0 - 1.0;
    vec3 n3 = texture(normalMap, uv3).rgb * 2.0 - 1.0;

    // Blend all three layers
    vec3 nBlend = normalize(n1 + n2 * 0.6 + n3 * 0.4);

    // Apply to vertex normal: for a horizontal plane tangent=X, bitangent=Z
    vec3 N = normalize(Normal);
    N = normalize(N + vec3(nBlend.x * 0.35, 0.0, nBlend.y * 0.35));

    // ---- 3. View direction -----------------------------------------------
    vec3 V    = normalize(cameraWorldPos - FragPos);
    float NdV = clamp(dot(N, V), 0.0, 1.0);

    // ---- 4. Sky reflection -----------------------------------------------
    vec3 R = reflect(-V, N);
    R.y = max(R.y, 0.001);
    vec3 skyRefl = proceduralSky(R);

    // ---- 4b. Planar reflection (scene geometry) --------------------------
    // ReflClipPos was computed in water.vs using the reflected camera's VP matrix,
    // so perspective-divide gives the correct UV in the reflection FBO.
    vec2 reflUV = (ReflClipPos.xy / ReflClipPos.w) * 0.5 + 0.5;
    // Small wave-normal distortion makes the reflection ripple with the waves
    reflUV += N.xz * 0.025;
    reflUV  = clamp(reflUV, 0.001, 0.999);

    // Blend: favour planar reflection but fall back to sky where FBO alpha = 0
    vec4  planarSample = texture(reflectionTexture, reflUV);
    float planarWeight = clamp(planarSample.a * 1.5, 0.0, 0.85);
    vec3  planarRefl   = planarSample.rgb;
    vec3  reflColor    = mix(skyRefl, planarRefl, planarWeight);

    // ---- 5. Fresnel (Schlick) --------------------------------------------
    float F0 = 0.04;
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdV, 5.0);
    fresnel = clamp(fresnel, 0.0, 0.95);

    // ---- 6. Water body color: trough → crest ----------------------------
    vec3 troughColor = waterColorDeep   * 0.7;
    vec3 crestColor  = waterColorShallow;
    vec3 waterColor  = mix(troughColor, crestColor, crestFactor);

    // ---- 7. Foam from texture (two scrolling layers, multiplied for masks)
    vec2 fuv1 = FragPos.xz * 0.08 + vec2( t * 0.025,  t * 0.018);
    vec2 fuv2 = FragPos.xz * 0.12 + vec2(-t * 0.020,  t * 0.030);
    float f1 = texture(foamTexture, fuv1).r;
    float f2 = texture(foamTexture, fuv2).r;

    // Intersection for organic foam streaks
    float foamMask = f1 * f2;
    foamMask = smoothstep(0.25, 0.65, foamMask);

    // Also add foam on wave crests (height-driven)
    float crestFoam = smoothstep(0.55, 0.85, crestFactor);
    float waveFoam  = clamp(foamMask + crestFoam * 0.5, 0.0, 1.0);

    // ---- 8. Specular highlight ------------------------------------------
    vec3  H    = normalize(sunDir + V);
    float spec = pow(clamp(dot(N, H), 0.0, 1.0), 80.0);
    vec3  specular = sunColor * spec * specStrength;

    // ---- 9. Combine ------------------------------------------------------
    // Strong base opacity so the water body colour dominates regardless of
    // what object may or may not sit behind the water surface.
    vec3 color = mix(waterColor, reflColor, fresnel * reflectStrength);

    vec3 foamColor = vec3(0.92, 0.96, 1.0);
    color = mix(color, foamColor, waveFoam * foamStrength);
    color += specular;

    // Raise base alpha to reduce see-through colour bleed from objects behind
    float alpha = mix(waterAlpha, min(waterAlpha + 0.10, 0.97), waveFoam * 0.6);

    FragColor = vec4(color, alpha);
}
