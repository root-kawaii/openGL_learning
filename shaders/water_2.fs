#version 330 core
in vec2 TexCoord;
in vec3 WorldPos;
in vec4 ClipSpacePos;

uniform sampler2D depthTexture;
uniform sampler2D foamTexture;
uniform vec3 cameraPos;
uniform float time;
uniform float nearPlane;
uniform float farPlane;

// Water properties
uniform vec3 waterColorShallow;
uniform vec3 waterColorDeep;
uniform vec3 waterColorMid;        // NEW: Middle water color
uniform float waterTransparency;
uniform float foamThreshold;
uniform float foamStrength;

// Cell shading parameters
uniform int depthBands;            // Number of depth color bands (e.g., 4)
uniform int foamBands;             // Number of foam intensity bands (e.g., 3)
uniform float cellShadingStrength; // How pronounced the banding is (0-1)

out vec4 FragColor;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

// Cell shading function - quantizes a value into discrete bands
float CellShade(float value, int bands)
{
    return floor(value * float(bands)) / float(bands);
}

// Smooth cell shading with optional smoothing between bands
float CellShadeSmooth(float value, int bands, float smoothness)
{
    float stepped = floor(value * float(bands)) / float(bands);
    return mix(stepped, value, smoothness);
}

// Create stylized wave patterns like in the reference image
float CreateStylizedWaves(vec2 worldXZ, float time)
{
    // Create flowing, organic wave patterns
    vec2 waveUV = worldXZ * 0.15;
    
    // Primary wave direction
    float wave1 = sin(waveUV.x * 2.0 + time * 0.8) * cos(waveUV.y * 1.5 + time * 0.6);
    
    // Secondary wave direction (perpendicular)
    float wave2 = sin(waveUV.y * 2.5 + time * 0.9) * cos(waveUV.x * 1.8 + time * 0.7);
    
    // Combine waves with different phases
    float combinedWaves = (wave1 + wave2 * 0.7) * 0.5 + 0.5;
    
    // Create flowing streaks (like in the reference)
    float streak1 = sin(waveUV.x * 3.0 + waveUV.y * 1.0 + time * 1.2);
    float streak2 = sin(waveUV.x * 1.5 + waveUV.y * 2.0 + time * 0.8);
    
    float streaks = (streak1 + streak2) * 0.25 + 0.5;
    
    // Combine everything
    return mix(combinedWaves, streaks, 0.6);
}

float CalculateCellShadedFoam(float depthDiff, vec2 worldXZ)
{
    // Create proximity-based foam (like around the boat)
    float foamDistance = 3.0;
    float foamMask = 1.0 - smoothstep(0.0, foamDistance, depthDiff);
    foamMask = pow(foamMask, 0.2); // Softer falloff
    
    // USE FOAM TEXTURE FOR SHAPE/PATTERN
    vec2 foamUV1 = worldXZ * 0.08 + time * 0.03;
    vec2 foamUV2 = worldXZ * 0.12 + time * -0.02;
    
    float foamPattern;
    float foam1 = texture(foamTexture, foamUV1).r;
    float foam2 = texture(foamTexture, foamUV2).r;
    
    if (foam1 < 0.1 && foam2 < 0.1) {
        // Fallback to procedural if no texture
        float swirl1 = sin(foamUV1.x * 4.0) * cos(foamUV1.y * 3.0);
        float swirl2 = sin(foamUV2.x * 2.5 + foamUV2.y * 1.5);
        foamPattern = (swirl1 + swirl2) * 0.5 + 0.5;
        foamPattern = smoothstep(0.3, 0.8, foamPattern);
    } else {
        // Use texture for organic foam shapes
        foamPattern = foam1 * foam2;
        foamPattern = smoothstep(0.2, 0.9, foamPattern);
    }
    
    // Sharp foam edges (like white caps)
    float sharpFoam = 1.0 - smoothstep(0.0, 0.5, depthDiff);
    sharpFoam = step(0.7, sharpFoam); // Very sharp cutoff
    
    float baseFoam = max(foamMask * foamPattern * foamStrength, sharpFoam * 0.9);
    
    // Cell shade the foam intensity into distinct levels
    return CellShade(baseFoam, foamBands);
}

void main()
{
    // Calculate screen UV
    vec2 screenUV = gl_FragCoord.xy / textureSize(depthTexture, 0);
    
    // Sample and linearize depths
    float sceneDepthRaw = texture(depthTexture, screenUV).r;
    float waterDepthRaw = gl_FragCoord.z;
    
    float sceneDepth = LinearizeDepth(sceneDepthRaw);
    float waterDepth = LinearizeDepth(waterDepthRaw);
    
    float depthDiff = sceneDepth - waterDepth;
    
    // STYLIZED WATER COLORING (like the reference image)
    float waterDepthFactor = clamp(depthDiff * 0.05, 0.0, 1.0);
    
    // Add wave-based color variation
    float waveInfluence = 0;
    float combinedDepth = mix(waterDepthFactor, waveInfluence, 0.3);
    
    // Create 4 distinct water zones (like in the reference)
    float cellShadedDepth = CellShade(combinedDepth, 4);
    
    vec3 waterColor;
    if (cellShadedDepth < 0.25) {
        // Very shallow - bright cyan
        waterColor = mix(vec3(0.4, 0.8, 1.0), vec3(0.3, 0.7, 0.95), waveInfluence);
    } else if (cellShadedDepth < 0.5) {
        // Shallow - medium blue
        waterColor = mix(vec3(0.2, 0.6, 0.9), vec3(0.25, 0.65, 0.85), waveInfluence);
    } else if (cellShadedDepth < 0.75) {
        // Medium - deeper blue
        waterColor = mix(vec3(0.15, 0.45, 0.8), vec3(0.1, 0.5, 0.75), waveInfluence);
    } else {
        // Deep - dark blue
        waterColor = mix(vec3(0.05, 0.3, 0.7), vec3(0.08, 0.35, 0.65), waveInfluence);
    }
    
    // SIMPLIFIED FRESNEL (more cartoon-like)
    vec3 viewDir = normalize(cameraPos - WorldPos);
    vec3 surfaceNormal = vec3(0.0, 1.0, 0.0);
    float fresnel = pow(1.0 - max(dot(viewDir, surfaceNormal), 0.0), 3.0);
    
    // Binary fresnel - either there or not (like in cartoon water)
    float cellShadedFresnel = step(0.4, fresnel);
    
    // Calculate stylized foam (using texture for shape)
    float foam = CalculateCellShadedFoam(depthDiff, WorldPos.xz);
    
    // APPLY CELL-SHADED COLORS TO FOAM SHAPES
    vec3 finalColor;
    if (foam > 0.66) {
        // Bright white foam caps (highest foam areas from texture)
        finalColor = vec3(0.95, 0.98, 1.0);
    } else if (foam > 0.33) {
        // Medium foam - mix with water color (medium foam areas from texture)
        finalColor = mix(waterColor, vec3(0.8, 0.9, 0.98), 0.7);
    } else if (foam > 0.1) {
        // Light foam tint (low foam areas from texture)
        finalColor = mix(waterColor, vec3(0.9, 0.95, 1.0), 0.3);
    } else {
        // Pure water color (no foam from texture)
        finalColor = waterColor;
    }
    
    // Add rim lighting (like cartoon water highlights)
    if (cellShadedFresnel > 0.5) {
        finalColor = mix(finalColor, vec3(0.7, 0.85, 1.0), 0.4);
    }
    
    // CARTOON-STYLE ALPHA (simple and clean)
    float baseAlpha = 0.7; // Base transparency
    
    // More opaque where there's foam
    if (foam > 0.3) {
        baseAlpha = 0.85;
    }
    
    // Slight transparency variation with depth
    baseAlpha += cellShadedDepth * 0.15;
    baseAlpha = clamp(baseAlpha, 0.6, 0.9);
    
    FragColor = vec4(finalColor, baseAlpha);
}